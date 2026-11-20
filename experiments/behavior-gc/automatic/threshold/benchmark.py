import hashlib
import os
from pathlib import Path
import random
import re
import subprocess
import sys

sys.path.extend(str(Path(__file__).resolve().parents[1] / folder) for folder in ('.', 'cost', 'lexer'))
from build import build
from lexer_time import DIGEST, timed_driver
from memory import boundary_charges, cycle_count, modules, validate_tokens, write_csv
from run import compile_fixture, native_calls, native_stock, ordered
from run_cost import ROUNDS, check_calls, checksum, output, verify_trace
from workload import fixture

here = Path(__file__).resolve().parent
KINDS = ('stock', 'eager', 'entry', 'late', 'inline')
FLAGS = ['--ponymaxthreads=1', '--ponynoscale']
HIGH = '--ponygcinitial=30'


def compile_case(builds, kind, source, dest, *, debug=False, extra=()):
    switches = {'GC_FRAME_ENTRY_THRESHOLD': kind == 'entry',
                'GC_FRAME_INLINE_CONTROL': kind == 'inline'}
    saved = {key: os.environ.pop(key, None) for key in switches}
    try:
        os.environ.update({key: '1' for key, enabled in switches.items() if enabled})
        compiler = builds['eager'] if kind == 'eager' else builds['candidate']
        exe = compile_fixture(compiler, source, dest, debug, kind != 'stock', extra, builds['stock'])
    finally:
        for key, old in saved.items():
            if old is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = old
    if 'Merging runtime' in (dest/'compile.log').read_text():
        raise ValueError('expected a native runtime archive')
    return exe


def orders(seed, kinds=KINDS):
    rng = random.Random(seed)
    for _ in range(3):
        variants = list(kinds)
        rng.shuffle(variants)
        rows = []
        for start in range(5):
            row = [variants[(start + i) % 5] for i in (0, 1, 4, 2, 3)]
            rows.extend([row, row[::-1]])
        rng.shuffle(rows)
        yield from rows


def trace_args(path):
    return ['--ponytracingcategories=actor_behavior,actor_gc', '--ponytracingoutput=' + str(path)]


def native_check(exe, kind):
    if kind in ('stock', 'eager'):
        check_calls(exe, 'stock' if kind == 'stock' else 'full')
        return
    dispatcher = native_calls(exe, 'Probe_Dispatch')
    mix, finish = 'Probe_ref_mix_o', 'Probe_ref_finish_o'
    if kind == 'inline':
        ordered(dispatcher, [mix, finish])
        if any(name.startswith(('ponyint_frame_', 'ponyint_threshold_')) for name in dispatcher):
            raise ValueError('instrumented inlining control')
        return
    symbol = 'ponyint_threshold_due'
    asm = (exe.parent/'Probe_Dispatch.asm').read_text()
    after = asm.split('<' + symbol + '>', 1)[1]
    branch = re.match(r'\s*[^\n]*\btest\s+%eax,%eax\n[^\n]*\bje\s+([0-9a-f]+)', after)
    if not branch:
        raise ValueError('expected a jump on a false threshold result')
    target = re.search(r'^\s*' + branch[1] + r':', after, re.M)
    if not target:
        raise ValueError('missing branch target')
    if kind == 'late':
        ordered(dispatcher, [mix, symbol, 'ponyint_threshold_collect', finish])
        if '<ponyint_threshold_collect>' not in after[:target.start()]:
            raise ValueError('false branch reaches collector')
        first_call = re.search(r'\bcallq?\s+[0-9a-f]+ <([^>]+)>', after[target.start():])
        if not first_call or first_call[1] != finish:
            raise ValueError('false branch should go to finish')
    else:
        middle = 'Probe_ref_middle_o'
        slow = after[:target.start()]
        if not all('<' + name + '>' in slow for name in
                   ('ponyint_frame_push', middle+'.armed', 'ponyint_frame_pop')):
            raise ValueError('missing armed call')
        first_call = re.search(r'\bcallq?\s+[0-9a-f]+ <([^>]+)>', after[target.start():])
        if not first_call or first_call[1] != middle:
            raise ValueError('false branch should go to ordinary helper')
        for name in [middle, 'Probe_ref_phase_o', mix, finish]:
            if any(call.startswith(('ponyint_frame_', 'ponyint_threshold_'))
                   for call in native_calls(exe, name)):
                raise ValueError('unconditional instrumentation in ' + name)


def micro(builds, out, *, kinds=KINDS, compile_one=compile_case, inspect=native_check, seed=6490):
    out.mkdir()
    expected = checksum(ROUNDS)
    exes = {}
    for kind in kinds:
        exe = compile_one(builds, kind, here.parent/'cost/fixture', out/kind)
        inspect(exe, kind)
        trace = exe.parent/'trace.json'
        output(exe, expected, trace_args(trace))
        verify_trace(trace)
        output(exe, expected, [])
        exes[kind] = exe
    rows = []
    for pair, kinds in enumerate(orders(seed, kinds)):
        for order, kind in enumerate(kinds):
            elapsed = output(exes[kind], expected, [])
            rows.append(dict(pair=pair, order=order, variant=kind, behaviors=ROUNDS,
                             elapsed_ns=elapsed, checksum=expected))
    write_csv(out/'micro.csv', rows)


def duration(data):
    tokens, marker, timer = data.rpartition(b'ELAPSED ')
    if not marker or hashlib.sha256(tokens).hexdigest() != DIGEST or int(timer) <= 0:
        raise ValueError('lexer output or timer')
    return tokens, int(timer)


def lexer(builds, out, *, kinds=KINDS, compile_one=compile_case, inactive=('stock', 'inline')):
    out.mkdir()
    inputs = {name: Path(name).read_bytes() for name in modules}
    src = out/'src'
    flags = fixture(src, inputs.items(), 16)
    timed_driver(src/'main.pony')
    timer = out/'time-child'
    subprocess.run(['cc', '-O2', '-Wall', '-Werror', here.parents[1]/'ordinary/time-child.c',
                    '-o', timer], check=True)
    charges, runs = [], []
    commands = {}
    for kind in kinds:
        exe = compile_one(builds, kind, src, out/kind, extra=flags)
        for level in ('default', 'high'):
            directory = exe.parent/level
            directory.mkdir()
            args = FLAGS + ([HIGH] if level == 'high' else [])
            commands[kind, level] = [exe, *args]
            trace = directory/'trace.json'
            tokens, _ = duration(subprocess.check_output([exe, *args, *trace_args(trace)]))
            count = validate_tokens(tokens, inputs, 16)
            cycles, ident = cycle_count(trace)
            if count != 4089 or (level == 'high' or kind in inactive) and cycles:
                raise ValueError((kind, level, count, cycles))
            start, end = boundary_charges(exe, args, ident, directory)
            charges.append(dict(variant=kind, threshold=level, tokens=count, cycles=cycles,
                                start_charge=start, end_charge=end))
            duration(subprocess.check_output(commands[kind, level]))
    write_csv(out/'lexer-memory.csv', charges)
    for pair, kinds in enumerate(orders(6491, kinds)):
        for level in (('default', 'high') if pair % 2 else ('high', 'default')):
            for order, kind in enumerate(kinds):
                r = subprocess.run([timer, *commands[kind, level]], capture_output=True, check=True)
                _, elapsed = duration(r.stdout)
                wall, rss = map(int, r.stderr.split())
                runs.append(dict(pair=pair, order=order, variant=kind, threshold=level,
                                 elapsed_ns=elapsed, process_ns=wall, peak_rss_kib=rss))
    write_csv(out/'lexer.csv', runs)


def compilers(out):
    out.mkdir()
    builds = {'stock': native_stock(out/'stock'), 'eager': out/'eager', 'candidate': out/'candidate'}
    build(builds['eager'])
    build(builds['candidate'], threshold_first=True)
    return builds


if __name__ == '__main__':
    work = Path(sys.argv[1]).resolve()
    builds = compilers(work/'compilers')
    from checks import safety
    safety(builds, work/'checks')
    micro(builds, work/'micro')
    lexer(builds, work/'lexer')
