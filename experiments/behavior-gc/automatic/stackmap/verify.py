import re
import subprocess

from benchmark import FLAGS, HIGH, trace_args, write_csv
from checks import debugger, mixed_fixture
from experiment import compile_one, require_sites
from inspect_maps import output, records
from memory import cycle_count
from safety import golden
from workload import fixture, TINY


def breakpoint(spec, body):
    return f'break {spec}\ncommands\nsilent\n{body}\ncontinue\nend\n'


def walk_check(exe, seed, level, actor_source):
    name = 'Probe_tag_unsupported_o' if seed == 138 else 'Probe_tag_measure_o'
    symbols = {n: int(a, 16) for a, n in re.findall(
        r'^([0-9a-f]+) [A-Za-z] (\S+)$', output('nm', '-n', exe), re.M)}
    maps = {pc: role for _, role, pc, _, _ in records(exe) if role != 1}
    script = f'starti\nset $base = (uintptr_t)&ponyint_mapped_collect - {symbols["ponyint_mapped_collect"]}\n'
    script += 'set $lookups = 0\n'
    for method in (name, 'Probe_ref_middle_o'):
        if method not in symbols:
            continue
        asm = output('objdump', '-d', '--no-show-raw-insn', '--disassemble='+method, exe)
        after_alloc = re.findall(r'\bcall\s+\w+ <pony_alloc_small>\n\s*([0-9a-f]+):', asm)
        for addr in after_alloc:
            script += breakpoint('*($base+0x'+addr+')',
                                 f'printf "ALLOC {method} %#lx\\n", $rax')
    script += breakpoint("*'Probe_ref_fill_o'", '''set $a = (pony_actor_t*)$rdi
printf "FILL %zu %zu\\n", $a->heap.used, $a->heap.next_gc''')
    equal = 'set actor->heap.next_gc = actor->heap.used\n' if level == 'equal' else ''
    script += breakpoint('ponyint_threshold_due', equal +
                         'printf "GUARD %zu %zu\\n", actor->heap.used, actor->heap.next_gc')
    script += breakpoint('map_lookup', '''set $lookups = $lookups + 1
printf "IP %#lx\\n", pc-$base''' + ('''
if $lookups == 2
  set pc = pc + 1
end''' if level == 'miss' else ''))
    line, = [i for i, s in enumerate(actor_source.read_text().splitlines(), 1)
             if 'if(walk.depth != 4)' in s]
    script += breakpoint(f'{actor_source}:{line}', 'printf "WALK %u\\n", walk.depth')
    script += breakpoint('frame_collect if count == 2',
                         'printf "ROOTS %#lx %#lx\\n", (uintptr_t)roots[0], (uintptr_t)roots[1]')
    opts = ' '.join(FLAGS + ([HIGH] if level == 'high' else []))
    # starti loads the PIE; change arguments before that launch.
    script = f'set args {seed} {opts}\n' + script + 'continue\n'
    log = debugger(exe, script, f'walk-{seed}-{level}')
    fill, = re.findall(r'^FILL (\d+) (\d+)$', log, re.M)
    guard, = re.findall(r'^GUARD (\d+) (\d+)$', log, re.M)
    before, limit = map(int, fill)
    charge, due = map(int, guard)
    if level in ('default', 'miss') and not before <= limit == due < charge:
        raise ValueError(('fixture failed to cross inside fill', fill, guard))
    if level == 'equal' and due != charge or level == 'high' and charge > due:
        raise ValueError((level, charge, due))
    pcs = [int(a, 16) for a in re.findall(r'^IP (0x\w+)$', log, re.M)]
    roles = [maps.get(pc, 0) for pc in pcs]
    depth = [int(n) for n in re.findall(r'^WALK (\d+)$', log, re.M)]
    roots = re.findall(r'^ROOTS (0x\w+) (0x\w+)$', log, re.M)
    expected = [] if level in ('high', 'equal') else (
        [4, 2] if level == 'miss' else [4, 2, 0 if seed == 138 else 3])
    expected_depth = [] if not expected else [2 if level == 'miss' else 3 if seed == 138 else 4]
    if roles != expected or depth != expected_depth:
        raise ValueError((roles, depth, log))
    if expected_depth == [4]:
        (outer, inner), = roots
        for method, pointer in [(name, outer), ('Probe_ref_middle_o', inner)]:
            first, *_ = re.findall(r'^ALLOC '+method+r' (0x\w+)$', log, re.M)
            if pointer != first:
                raise ValueError(('caller allocation differs from recovered root', first, pointer))
        if outer == inner:
            raise ValueError('duplicate root')
    elif roots:
        raise ValueError('collection after refusal')
    return dict(seed=seed, threshold=level, before=before, initial_limit=limit, after=charge,
                limit_at_poll=due, map_roles=' '.join(map(str, roles)), recovered=len(roots)*2)


def safety(builds, out):
    out.mkdir()
    mixed_fixture(out/'mixed')
    results, walks = [], []
    for debug in (False, True):
        mode = 'debug' if debug else 'optimized'
        exe = compile_one(builds, 'mapped', out/'mixed', out/mode, debug=debug)
        require_sites(exe, 'mapped', 1)
        for seed, level in [(137,'default'), (138,'default'), (139,'default'), (137,'high')]:
            name = 'Probe.unsupported' if seed == 138 else 'Probe.measure'
            path = exe.parent/f'{seed}-{level}.json'
            args = FLAGS + ([HIGH] if level == 'high' else [])
            data = subprocess.check_output([exe, str(seed), *args, *trace_args(path)], text=True).strip()
            if data != f'{seed+5} {seed+18} {seed+1}':
                raise ValueError(data)
            cycles, _ = cycle_count(path, name)
            if cycles != int(seed != 138 and level == 'default'):
                raise ValueError((seed, level, cycles))
            results.append(dict(codegen=mode, case=seed, threshold=level, cycles=cycles))
        for seed, level in [(137,'default'), (138,'default'), (137,'high'), (137,'equal'), (137,'miss')]:
            row = walk_check(exe, seed, level, builds['mapped']/'actor.c')
            walks.append(dict(codegen=mode, **row))
        from benchmark import here
        text = debugger(exe, (here.parent/'marks.gdb').read_text(), 'marks')
        if 'Observed marking for both holders' not in text:
            raise ValueError('missing marks')

    for variant in ('unarmed', 'finalizer'):
        src = out/(variant+'-src')
        flags = fixture(src, [('unit.pony', TINY)], variant=variant)
        exe = compile_one(builds, 'mapped', src, out/variant, extra=flags)
        trace = exe.parent/'trace.json'
        golden(subprocess.check_output([exe, *FLAGS, *trace_args(trace)]))
        cycles, _ = cycle_count(trace)
        script = breakpoint('map_lookup', 'printf "LOOKUP\\n"')
        script += breakpoint('ponyint_threshold_collect', 'printf "COMPLETE %zu %zu\\n", actor->heap.used, actor->heap.next_gc')
        log = debugger(exe, script+'run --ponymaxthreads=1 --ponynoscale\n', 'refusal')
        calls = re.findall(r'^COMPLETE (\d+) (\d+)$', log, re.M)
        if cycles or len(calls) != int(variant == 'finalizer') or 'LOOKUP\n' not in log:
            raise ValueError((variant, calls, cycles, log))
        if calls and int(calls[0][0]) <= int(calls[0][1]):
            raise ValueError('expected collection due')
        results.append(dict(codegen='optimized', case=variant, threshold='default', cycles=cycles))
    write_csv(out/'safety.csv', results)
    write_csv(out/'walks.csv', walks)


def lexer_slots(exe, actor_source):
    nm = output('nm', '-n', exe)
    slide_anchor, = re.findall(r'^([0-9a-f]+) T ponyint_mapped_collect$', nm, re.M)
    script = ('set args --ponymaxthreads=1 --ponynoscale\nstarti > /dev/null\n'
              + f'set $base = (uintptr_t)&ponyint_mapped_collect - 0x{slide_anchor}\n')
    for caller, callee in [('outer','Probe_ref_middle_o'), ('inner','Probe_ref_phase_o')]:
        script += breakpoint("*'"+callee+"'", f'printf "ENTRY {caller} %#lx %#lx\\n", $rbp, $rsp+8')
    for caller, name in [('outer','Probe_tag_measure_o'), ('inner','Probe_ref_middle_o')]:
        asm = output('objdump', '-d', '--no-show-raw-insn', '--disassemble='+name, exe)
        for pc in re.findall(r'\bcall\s+\w+ <pony_alloc_small>\n\s*([0-9a-f]+):', asm):
            script += breakpoint('*($base+0x'+pc+')', f'printf "ALLOC {caller} %#lx\\n", $rax')
    line, = [n for n, code in enumerate(actor_source.read_text().splitlines(), 1)
             if 'if(fp < sp ||' in code]
    script += breakpoint(f'{actor_source}:{line}',
                         'printf "LOC %u %#lx %#lx %u %d\\n", walk->depth, fp, sp, record.reg, record.offset')
    line, = [n for n, code in enumerate(actor_source.read_text().splitlines(), 1)
             if 'if(walk.depth != 4)' in code]
    script += breakpoint(f'{actor_source}:{line}',
                         'printf "SLOTS %u %#lx %#lx\\n", walk.depth, walk.slots[0], walk.slots[1]')
    script += breakpoint('frame_collect if count == 2',
        'printf "BATCH %#lx %#lx %d %d\\n", (uintptr_t)roots[0], (uintptr_t)roots[1], '
        '(*(void**)roots[0] == (void*)&Batch_Desc), (*(void**)roots[1] == (void*)&Batch_Desc)')
    log = debugger(exe, script+'continue\n', 'lexer-slots')
    entries = {}
    allocs = {}
    for caller in ('outer','inner'):
        (fp, sp), = re.findall(r'^ENTRY '+caller+r' (0x\w+) (0x\w+)$', log, re.M)
        entries[caller] = (int(fp, 16), int(sp, 16))
        allocs[caller] = set(re.findall(r'^ALLOC '+caller+r' (0x\w+)$', log, re.M))
    locs = re.findall(r'^LOC ([23]) (0x\w+) (0x\w+) ([67]) (-?\d+)$', log, re.M)
    slots = re.findall(r'^SLOTS 4 (0x\w+) (0x\w+)$', log, re.M)
    batches = re.findall(r'^BATCH (0x\w+) (0x\w+) 1 1$', log, re.M)
    if (len(locs), len(slots), len(batches)) != (32,16,16):
        raise ValueError('incomplete lexer frame observations')
    rows = []
    for i in range(16):
        outer, inner = batches[i]
        if outer == inner or outer not in allocs['outer'] or inner not in allocs['inner']:
            raise ValueError('expected two distinct batches allocated in their respective callers')
        for j, caller in enumerate(('inner','outer')):
            depth, fp, sp, reg, offset = locs[i*2+j]
            frame = int(fp,16), int(sp,16)
            slot = frame[0 if reg=='6' else 1] + int(offset)
            if reg != '7' or frame != entries[caller] or int(depth) != j+2 or slot != int(slots[i][j],16):
                raise ValueError(('unwind did not match suspended caller registers', caller, i))
            rows.append(dict(poll=i+1, caller=caller, register=reg, offset=offset,
                             caller_frame_matches=1, slot_matches=1, batch_matches=1))
    write_csv(exe.parent/'lexer-slots.csv', rows)
