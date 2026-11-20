import re
import shutil
import subprocess

from benchmark import FLAGS, HIGH, compile_case, here, trace_args
from build import replace
from memory import cycle_count, write_csv
from run import native_calls
from safety import golden
from workload import fixture, TINY


def mixed_fixture(src):
    shutil.copytree(here.parent/'fixture', src)
    (src/'fixture.pony').rename(src/(src.name+'.pony'))
    path = src/'main.pony'
    s = replace(path.read_text(), '=> bytes = Array[U64].init(n, 3)',
                '=>\n    bytes = Array[U64].init(n, if n == 139 then 8192 else 3 end)')
    s = replace(s, '  var _sum:', '  var _extra: (Holder | None) = None\n  var _sum:')
    s = replace(s, '    measure()\n', '''    if (n % 2) == 0 then unsupported() else measure() end

  be unsupported() =>
    let extra = Holder(_seed + 21)
    let outer = Holder(_seed)
    middle()
    _extra = extra
    _outer = outer
    verify()
''')
    s = replace(s, '      let a =', '''      if (_seed % 2) == 0 then
        if (_extra as Holder).bytes(2)? != (_seed + 21) then return end
      end
      let a =''')
    path.write_text(s)


def debugger(exe, text, label):
    path = exe.parent/(label+'.gdb')
    path.write_text('set confirm off\nset pagination off\n' + text)
    r = subprocess.run(['gdb', '-nx', '-batch', '-x', path, exe],
                       text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    (exe.parent/(label+'.log')).write_text(r.stdout)
    if r.returncode or 'exited normally' not in r.stdout:
        raise ValueError(r.stdout)
    return r.stdout


def charges(exe, kind, seed, level):
    armed = kind == 'entry' and seed == 139 and level == 'default'
    suffix = '.armed' if armed else ''
    program = ''
    for tag, symbol in [('FILL', 'Probe_ref_fill_o'+suffix),
                        ('FINISH', 'Probe_ref_finish_o'+suffix),
                        ('GUARD', 'ponyint_threshold_due'),
                        ('SLOW', 'ponyint_threshold_collect'),
                        ('PUSH', 'ponyint_frame_push')]:
        program += f"break *'{symbol}'\ncommands\nsilent\nset $a = (pony_actor_t*)$rdi\n"
        if level == 'equal' and tag == 'GUARD':
            program += 'set $a->heap.next_gc = $a->heap.used\n'
        program += f'printf "EVENT {tag} %zu %zu\\n", $a->heap.used, $a->heap.next_gc\n'
        if tag == 'FILL':
            program += '''python
f = gdb.newest_frame()
while f is not None:
    if f.type() != gdb.INLINE_FRAME:
        print('NATIVE', gdb.execute('info symbol %#x' % f.pc(), to_string=True).split()[0])
    f = f.older()
end
'''
        program += 'continue\nend\n'
    args = ' '.join(FLAGS + ([HIGH] if level == 'high' else []))
    result = debugger(exe, program+f'run {seed} {args}\n', f'{seed}-{level}')
    events = [(tag, int(used), int(limit)) for tag, used, limit in
              re.findall(r'^EVENT (\w+) (\d+) (\d+)$', result, re.M)]
    tags = [tag for tag, _, _ in events]
    expected = ['FILL', 'FINISH']
    if seed != 138:
        if kind == 'entry':
            expected = ['GUARD'] + (['PUSH', 'PUSH'] if armed else []) + expected
        else:
            expected = ['FILL', 'GUARD'] + (['SLOW'] if level == 'default' else []) + ['FINISH']
    if tags != expected:
        raise ValueError((kind, seed, level, events, expected))
    data = {tag: (used, limit) for tag, used, limit in events}
    if seed in (137, 138) and level == 'default':
        before, limit = data['FILL']
        after, end_limit = data.get('GUARD') if kind == 'late' and seed == 137 else data['FINISH']
        if not before <= limit < after or end_limit != limit:
            raise ValueError(('crossing', data))
    if 'GUARD' in data:
        used, limit = data['GUARD']
        if (level == 'equal' and used != limit) or ((used > limit) !=
                (level == 'default' and (kind == 'late' or armed))):
            raise ValueError(('guard', data))
    names = re.findall(r'^NATIVE (\S+)', result, re.M)
    for name in ('Probe_ref_middle_o', 'Probe_ref_phase_o'):
        if (name + suffix in names) != (kind == 'entry' or seed == 138):
            raise ValueError(('physical stack', names))
    return events


def safety(builds, out):
    out.mkdir()
    mixed_fixture(out/'mixed')
    rows, event_rows = [], []
    for kind in ('entry', 'late'):
        for mode in ('debug', 'optimized'):
            exe = compile_case(builds, kind, out/'mixed', out/(kind+'-'+mode), debug=mode=='debug')
            for seed, level in [(137, 'default'), (138, 'default'), (139, 'default'), (137, 'high')]:
                trace = exe.parent/f'{seed}-{level}.json'
                args = FLAGS + ([HIGH] if level == 'high' else [])
                result = subprocess.check_output([exe, str(seed), *args, *trace_args(trace)], text=True).strip()
                if result != f'{seed+5} {seed+18} {seed+1}':
                    raise ValueError(result)
                name = 'Probe.unsupported' if seed == 138 else 'Probe.measure'
                cycles, _ = cycle_count(trace, name)
                if cycles != int(level == 'default' and (seed == 139 or kind == 'late' and seed == 137)):
                    raise ValueError((kind, mode, seed, level, cycles))
                rows.append(dict(variant=kind, codegen=mode, case=str(seed), threshold=level, cycles=cycles))
            if mode == 'optimized':
                for seed, level in [(137, 'default'), (138, 'default'), (139, 'default'),
                                    (137, 'high'), (137, 'equal')]:
                    for tag, used, limit in charges(exe, kind, seed, level):
                        event_rows.append(dict(variant=kind, seed=seed, threshold=level, event=tag,
                                               used=used, next_gc=limit))
            mark_seed = 139 if kind == 'entry' else 137
            marks = (here.parent/'marks.gdb').read_text().replace('run 137', 'run '+str(mark_seed))
            if 'Observed marking for both holders' not in debugger(exe, marks, 'marks'):
                raise ValueError('missing root observations')
    src = out/'argument_local'
    shutil.copytree(here.parent/'fixture', src)
    (src/'fixture.pony').rename(src/(src.name+'.pony'))
    path = src/'main.pony'
    path.write_text(replace(path.read_text(), 'let outer = Holder(_seed)\n    middle()',
        'let outer =\n      Holder(\n        let value = _seed\n        value)\n'
        '    middle()\n    if value != _seed then return end'))
    for kind in ('entry', 'late'):
        exe = compile_case(builds, kind, src, out/('argument-'+kind))
        calls = native_calls(exe, 'Probe_Dispatch') + native_calls(exe, 'Probe_tag_measure_o')
        if 'ponyint_threshold_due' in calls:
            raise ValueError('admitted a constructor argument with its own declaration')
        trace = exe.parent/'trace.json'
        result = subprocess.check_output([exe, '137', *FLAGS, *trace_args(trace)], text=True).strip()
        cycles, _ = cycle_count(trace)
        if result != '142 155 138' or cycles:
            raise ValueError((kind, result, cycles))
        rows.append(dict(variant=kind, codegen='optimized', case='argument-local',
                         threshold='default', cycles=cycles))
    for variant in ('unarmed', 'finalizer'):
        src = out/(variant+'-src')
        flags = fixture(src, [('unit.pony', TINY)], variant=variant)
        exe = compile_case(builds, 'late', src, out/variant, extra=flags)
        trace = exe.parent/'trace.json'
        stream = subprocess.check_output([exe, *FLAGS, *trace_args(trace)])
        golden(stream)
        cycles, _ = cycle_count(trace)
        if cycles:
            raise ValueError((variant, cycles))
        text = debugger(exe, '''break ponyint_threshold_collect
commands
silent
printf "REFUSAL %zu %zu\\n", actor->heap.used, actor->heap.next_gc
continue
end
run --ponymaxthreads=1 --ponynoscale
''', 'refusal')
        hits = re.findall(r'^REFUSAL (\d+) (\d+)$', text, re.M)
        if variant == 'finalizer':
            if len(hits) != 1 or int(hits[0][0]) <= int(hits[0][1]):
                raise ValueError((variant, hits))
        elif hits:
            raise ValueError((variant, hits))
        rows.append(dict(variant='late', codegen='optimized', case=variant,
                         threshold='default', cycles=cycles))
    write_csv(out/'safety.csv', rows)
    write_csv(out/'crossings.csv', event_rows)
