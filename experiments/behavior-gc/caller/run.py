import argparse
import csv
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

source = Path(__file__).resolve().parent
forced = source.parent / 'forced'


def check(ok, message):
    if not ok:
        raise ValueError(message)


def check_record(row, mode, high, cycles):
    collect = mode == 'on' and not high
    check(row['allocations'] == 1024, 'allocation count')
    check(row['threshold'] == 1 << (30 if high else 14), 'runtime threshold')
    check(row['before'] <= row['threshold'] and row['before'] < row['poll'],
          'helper increased heap charge')
    check((row['poll'] > row['threshold']) == (not high), 'threshold crossing')
    check(row['due'] == int(not high), 'threshold decision')
    check(row['admitted'] == int(mode != 'unarmed'), 'root admission')
    check(row['completed'] == cycles == int(collect), 'collection and trace')
    check(row['after'] == (row['before'] if collect else row['poll']),
          'charge after poll')
    check(row['reused'] == int(collect), 'garbage slot reuse')
    check((row['kept'], row['mutated'], row['fresh']) == (137, 142, 211),
          'payloads after return')


def check_calls(directory):
    binary = directory / 'measure'
    functions = ['Probe_Dispatch', 'Probe_tag_measure_pWo', 'Probe_ref_phase_o',
                 'Probe_ref_allocate_garbage_o', 'caller_poll']
    calls = {}
    for name in functions:
        assembly = subprocess.check_output(
            ['objdump', '-d', '--disassemble=' + name, binary], text=True)
        (directory / (name + '.asm')).write_text(assembly)
        calls[name] = re.findall(r'\bcallq?\s+[0-9a-f]+ <([^>]+)>', assembly)
    phase = 'Probe_ref_phase_o'
    callers = [name for name in functions[:2] if phase in calls[name]]
    check(len(callers) == 1, 'one native caller of ref phase')
    caller = callers[0]
    sequence = calls[caller]
    position = sequence.index(phase)
    check('ponyint_gc_root_audit' in sequence[:position], 'audit before call')
    remaining = sequence[position + 1:]
    for target in ['ponyint_gc_root_audit', 'caller_addresses', 'caller_result']:
        check(target in remaining, 'caller resumes through ' + target)
        remaining = remaining[remaining.index(target) + 1:]
    targets = ['Probe_ref_allocate_garbage_o', 'caller_poll', 'caller_leave_phase']
    check([t for t in calls[phase] if t in targets] == targets, 'native phase calls')
    check('caller_garbage' in calls[targets[0]], 'native allocation observer')
    check('probe_collect' in calls['caller_poll'], 'native collector call')
    return caller


def counterfactuals(row, mode, high, cycles):
    for key in row:
        changed = dict(row)
        changed[key] = row['poll'] + 1 if key in ('before', 'after') else -1
        try:
            check_record(changed, mode, high, cycles)
        except ValueError:
            continue
        raise ValueError('corrupted measurement accepted: ' + key)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('output', type=Path)
    parser.add_argument('--builder', type=Path, default=forced / 'build.py')
    args = parser.parse_args()
    trial = args.output.resolve()
    trial.mkdir()  # A fresh directory keeps traces and binaries from previous runs out.
    shutil.copytree(source / 'fixture', trial / 'fixture')
    shutil.copy2(source / 'probe.c', trial)
    subprocess.run([sys.executable, args.builder, '--trial', trial,
                    '--site-marker', '    phase()', '--site-token', '('], check=True)
    for codegen in ['debug', 'optimized']:
        out = trial / codegen
        out.mkdir()
        compiler = [trial / 'bin/ponyc', '--gc-root-audit', '--pic', '-o', out,
                    '-b', 'measure', trial / 'fixture']
        if codegen == 'debug':
            compiler.insert(1, '-d')
        subprocess.run(compiler, check=True)
        caller = check_calls(out)
        with (out / 'runs.csv').open('w', newline='') as file:
            writer = None
            for threads in [1, 2]:
                for mode, high in [('on', False), ('off', False), ('on', True),
                                   ('unarmed', False)]:
                    stem = f'{threads}-{mode}-{"high" if high else "default"}'
                    trace = out / (stem + '.json')
                    cmd = [out / 'measure', f'--ponymaxthreads={threads}', '--ponynoscale',
                           '--ponytracingcategories=actor_behavior,actor_gc',
                           '--ponytracingoutput=' + str(trace)]
                    if high:
                        cmd.append('--ponygcinitial=30')
                    output = subprocess.check_output(cmd, env={**os.environ, 'CALLER_MODE': mode},
                                                     text=True).strip()
                    (out / (stem + '.txt')).write_text(output + '\n')
                    row = {k: int(v) for k, v in (item.split('=') for item in output.split())}
                    events = json.loads(trace.read_text())
                    cycles = check_trace(events, mode == 'on' and not high)
                    check_record(row, mode, high, cycles)
                    counterfactuals(row, mode, high, cycles)
                    record = dict(codegen=codegen, threads=threads, mode=mode,
                                  high=int(high), caller=caller, cycles=cycles, **row)
                    if writer is None:
                        writer = csv.DictWriter(file, fieldnames=record, lineterminator='\n')
                        writer.writeheader()
                    writer.writerow(record)
                    print(codegen, stem, output, flush=True)


if __name__ == '__main__':
    sys.path.insert(0, str(forced))
    from trace import check_trace
    main()
