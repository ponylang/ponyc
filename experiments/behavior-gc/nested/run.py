import argparse
import csv
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

from compiler import build_compiler

source = Path(__file__).resolve().parent
forced = source.parent / 'forced'
sys.path.insert(0, str(forced))
from trace import check_trace


def inspect_calls(out):
    def calls(name):
        assembly = subprocess.check_output(
            ['objdump', '-d', '--disassemble=' + name, out / 'measure'], text=True)
        (out / (name + '.asm')).write_text(assembly)
        return re.findall(r'\bcallq?\s+[0-9a-f]+ <([^>]+)>', assembly)

    def ordered(actual, required):
        for target in required:
            if target not in actual:
                raise ValueError(f'missing native call to {target} in {actual}')
            actual = actual[actual.index(target) + 1:]

    audit = 'ponyint_gc_root_audit'
    middle = 'Probe_ref_middle_o'
    phase = 'Probe_ref_phase_o'
    allocate = 'Probe_ref_allocate_garbage_o'
    candidates = [(name, calls(name)) for name in ['Probe_Dispatch', 'Probe_tag_measure_pWo']]
    (caller, body), = [(name, seq) for name, seq in candidates if middle in seq]
    ordered(body, [audit, middle, audit, 'nested_value'])
    ordered(calls(middle), [audit, phase, audit, 'nested_fresh', 'nested_value'])
    ordered(calls(phase), [allocate, 'nested_poll', 'nested_leave_phase'])
    ordered(calls(allocate), ['nested_garbage'])
    ordered(calls('nested_poll'), ['probe_collect_roots'])
    return caller


def check_record(row, mode, high, cycles):
    collect = int(mode == 'on' and not high)
    published = {'omit-outer': 2, 'omit-inner': 1}.get(mode, 3)
    expected = dict(outer=32, inner=64, poll=32832, threshold=1 << (30 if high else 14),
                    after=64 if collect else 32832, count=1024, published=published,
                    marked=3 * collect, completed=collect, reused=collect, outer_old=137,
                    outer_new=142, inner_old=271, inner_new=278)
    if row != expected or cycles != collect:
        raise ValueError(f'{mode} high={high}: trace cycles={cycles}, observed {row}, '
                         f'expected {collect} cycles and {expected}')


def measure(out, threads, mode, high):
    stem = f'{threads}-{mode}-{"high" if high else "default"}'
    trace = out / (stem + '.json')
    cmd = [out / 'measure', f'--ponymaxthreads={threads}', '--ponynoscale',
           '--ponytracingcategories=actor_behavior,actor_gc', '--ponytracingoutput=' + str(trace)]
    if high:
        cmd.append('--ponygcinitial=30')
    result = subprocess.check_output(cmd, env={**os.environ, 'NESTED_MODE': mode}, text=True)
    (out / (stem + '.txt')).write_text(result)
    row = {k: int(v) for k, v in (item.split('=') for item in result.split())}
    cycles = check_trace(json.loads(trace.read_text()), mode == 'on' and not high)
    check_record(row, mode, high, cycles)
    for field in row:
        changed = row.copy()
        changed[field] ^= 1
        try:
            check_record(changed, mode, high, cycles)
        except ValueError:
            continue
        raise ValueError(f'oracle accepted changed {field}')
    return dict(cycles=cycles, **row)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('output', type=Path)
    p.add_argument('--config', type=Path, default=Path('build/build_debug'))
    p.add_argument('--runtime-builder', type=Path, default=forced / 'build.py')
    args = p.parse_args()
    trial = args.output.resolve()
    trial.mkdir()
    shutil.copytree(source / 'fixture', trial / 'fixture')
    shutil.copy2(source / 'probe.c', trial)
    lines = (trial / 'fixture/main.pony').read_text().splitlines()
    (line, column), = [(idx, text.index('(') + 1) for idx, text in enumerate(lines, 1)
                       if text.strip() == 'phase()']
    (trial / 'inner_site.h').write_text(f'#define INNER_LINE {line}\n#define INNER_COLUMN {column}\n')
    subprocess.run([sys.executable, args.runtime_builder, '--trial', trial,
                    '--site-marker', '    middle()', '--site-token', '('], check=True)
    build_compiler(trial, args.config)
    records = []
    for codegen in ['debug', 'optimized']:
        out = trial / codegen
        out.mkdir()
        cmd = [trial / 'bin/ponyc', '--gc-root-audit', '--pic', '-o', out,
               '-b', 'measure', trial / 'fixture']
        if codegen == 'debug':
            cmd.insert(1, '-d')
        subprocess.run(cmd, check=True)
        caller = inspect_calls(out)
        for threads in [1, 2]:
            for mode, high in [('on', False), ('off', False), ('on', True),
                               ('omit-outer', False), ('omit-inner', False)]:
                data = measure(out, threads, mode, high)
                records.append(dict(codegen=codegen, threads=threads, mode=mode,
                                    high=int(high), caller=caller, **data))
                print(codegen, threads, mode, high, data, flush=True)
    with (trial / 'runs.csv').open('w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=records[0], lineterminator='\n')
        writer.writeheader()
        writer.writerows(records)


if __name__ == '__main__':
    main()
