import csv
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

here = Path(__file__).resolve().parent
sys.path.insert(0, str(here.parent / 'forced'))
from trace import check_trace


def compile_fixture(build, src, out, debug=False, enabled=True, extra=()):
    out.mkdir()
    env = os.environ.copy()
    env.pop('GC_FRAME_SOURCE', None)
    if enabled:
        env['GC_FRAME_SOURCE'] = str(src / 'main.pony')
    cmd = [build / 'bin/ponyc' if enabled else Path('build/debug/ponyc').resolve(),
           *(['-d'] if debug else []), '--pic', *extra, '-o', out, '-b', 'work', src]
    with (out / 'compile.log').open('w') as log:
        r = subprocess.run(cmd, env=env, stdout=log, stderr=subprocess.STDOUT)
    if r.returncode:
        raise RuntimeError((out / 'compile.log').read_text())
    return out / 'work'


def native_calls(binary, name):
    asm = subprocess.check_output(['objdump', '-d', '--disassemble='+name, binary], text=True)
    (binary.parent / (name + '.asm')).write_text(asm)
    return re.findall(r'\bcallq?\s+[0-9a-f]+ <([^>]+)>', asm)


def ordered(calls, sequence):
    for target in sequence:
        if target not in calls:
            raise ValueError((target, calls))
        calls = calls[calls.index(target) + 1:]


def trial(build, out):
    out.mkdir()
    results = []
    for variant in ['enabled', 'stock', 'omit-outer', 'omit-inner']:
        src = out / ('src-' + variant)
        shutil.copytree(here / 'fixture', src)
        main = src / 'main.pony'
        if variant.startswith('omit-'):
            name = variant.split('-')[1]
            text = main.read_text().replace('    let '+name+' =', '    let padding: U64 = 0\n    let '+name+' =')
            main.write_text(text)
        for mode in ['debug', 'optimized']:
            exe = compile_fixture(build, src, out / (variant+'-'+mode), mode == 'debug', variant != 'stock')
            if variant == 'enabled':
                middle, phase = 'Probe_ref_middle_o', 'Probe_ref_phase_o'
                callers = [native_calls(exe, n) for n in ['Probe_tag_measure_o','Probe_Dispatch']]
                caller, = [calls for calls in callers if middle in calls]
                ordered(caller, ['ponyint_frame_push', middle, 'ponyint_frame_pop'])
                ordered(native_calls(exe, middle), ['ponyint_frame_claim', 'ponyint_frame_push', phase, 'ponyint_frame_pop'])
                ordered(native_calls(exe, phase), ['ponyint_frame_claim', 'Probe_ref_fill_o', 'ponyint_frame_poll', 'Probe_ref_finish_o'])
                if any('root_audit' in call for call in caller):
                    raise ValueError('diagnostic call')
            for threads in [1, 2]:
                for high in [False, True] if variant == 'enabled' else [False]:
                    seed = 133 + threads
                    trace = exe.parent / f'{threads}-{int(high)}.json'
                    cmd = [exe, str(seed), f'--ponymaxthreads={threads}', '--ponynoscale',
                           '--ponytracingcategories=actor_gc,actor_behavior','--ponytracingoutput='+str(trace)]
                    if high:
                        cmd.append('--ponygcinitial=30')
                    output = subprocess.check_output(cmd, text=True).strip()
                    if output != f'{seed+5} {seed+18} {seed+1}':
                        raise ValueError(output)
                    cycles = check_trace(json.loads(trace.read_text()), variant == 'enabled' and not high)
                    results.append((variant, mode, threads, int(high), cycles, output))
    with (out / 'runs.csv').open('w', newline='') as f:
        w = csv.writer(f, lineterminator='\n')
        w.writerow(['variant','codegen','threads','raised_threshold','cycles','values'])
        w.writerows(results)


if __name__ == '__main__':
    trial(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())
