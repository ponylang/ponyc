#!/usr/bin/env python3

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


HERE = Path(__file__).resolve().parent
ROLES = ('debug-configure', 'debug-build', 'core-tests', 'optimized-tests',
         'tools-build', 'tool-tests', 'release-configure', 'release-build',
         'compile-hello', 'run-hello', 'inspect-interpreter',
         'compile-testprog', 'run-testprog')
LEAF = '''import json,os,pathlib,sys
name = pathlib.Path(sys.argv[0]).name
args = sys.argv[1:]
if name == 'cmake':
    preset = args[args.index('--preset') + 1]
    mode = 'release' if preset.endswith('release') else 'debug'
    role = 'tools-build' if '--target' in args else mode + (
        '-build' if '--build' in args else '-configure')
elif name == 'ctest':
    role = ('core-tests' if 'ci-core' in args else
            'tool-tests' if 'tools' in args else 'optimized-tests')
elif name == 'ponyc':
    program = args[args.index('-b') + 1]
    role = 'compile-' + program
elif name == 'readelf': role = 'inspect-interpreter'
else:
    assert name in ('hello','testprog'), name
    role = 'run-' + name
event = {'role':role, 'argv':args, 'cwd':os.getcwd(),
         'PONY_DEBUG':os.environ.get('PONY_DEBUG')}
event.update({key:os.environ[key] for key in
              ('HOME','TMPDIR','TMP','TEMP','TEMPDIR')})
if name == 'ponyc':
    output = pathlib.Path(args[args.index('-o') + 1]) / program
    event.update(output=str(output), existed=output.exists(),
                 source=(pathlib.Path(args[0]) / 'main.pony').read_text())
with open(os.environ['RECORD'],'a') as record:
    record.write(json.dumps(event) + '\\n')
status = json.loads(os.environ['RESULTS']).get(role,0)
if status: sys.exit(status)
if name == 'ponyc' and os.environ.get('NO_OUTPUT') != role:
    output.symlink_to(os.environ['LEAF'])
if name == 'readelf':
    assert pathlib.Path(args[1]).is_file(), args
    print('[Requesting program interpreter: ' +
          os.environ.get('INTERPRETER','/nix/store/controlled/ld.so') + ']')
'''



def require(condition, message):
    if not condition:
        raise AssertionError(message)


def run(root, script, results=None, extra=None):
    record = root / 'record.jsonl'
    record.write_text('')
    environment = dict(os.environ, PATH=str(root / 'bin') + ':' + os.environ['PATH'],
                       TMPDIR=str(root / 'temporary'), NIX_CC=str(root / 'toolchain'),
                       PONY_DEBUG='1', RECORD=str(record),
                       RESULTS=json.dumps(results or {}), LEAF=str(root / 'leaf.py'),
                       HOME=str(root / 'caller-home'), TMP=str(root / 'caller-tmp'),
                       TEMP=str(root / 'caller-temp'),
                       TEMPDIR=str(root / 'caller-tempdir'))
    environment.update(extra or {})
    entry = root / 'work/.ci-scripts/nix/nix-build-and-test.sh'
    entry.write_text(script)
    result = subprocess.run(['bash', str(entry)], cwd=root / 'work',
                            env=environment, text=True, capture_output=True, timeout=15)
    events = [json.loads(line) for line in record.read_text().splitlines()]
    require((root / 'temporary/unrelated/keep').read_text() == 'preserved',
            'source workload altered existing temporary contents')
    require(list((root / 'temporary').iterdir()) == [root / 'temporary/unrelated'],
            'source workload left its temporary programs behind')
    return result, events


def workload(events):
    return [event for event in events if event['role'] in ROLES]


def validate(root, events):
    events = workload(events)
    require(tuple(event['role'] for event in events) == ROLES,
            'source operation selection/order')
    for event in events:
        require(event['cwd'] == str(root / 'work'), event['role'] + ': working directory')
    selected = {event['role']: event for event in events}
    for mode in ('debug', 'release'):
        require(selected[mode + '-configure']['argv'] == [
            '--preset', 'x86-64-' + mode, '-DCMAKE_C_COMPILER=gcc',
            '-DCMAKE_CXX_COMPILER=g++'], mode + ': configured GCC compilers')
        require(selected[mode + '-build']['argv'] == [
            '--build', '--preset', 'x86-64-' + mode], mode + ': build preset')
    require(selected['tools-build']['argv'] == [
        '--build', '--preset', 'x86-64-debug', '--target', 'tool-tests'],
        'source tool test binaries build')
    for role, arguments, debug in (
        ('core-tests', ['-L', 'ci-core'], '1'),
        ('optimized-tests', ['-R', '^(stdlib|full-programs)$'], None),
        ('tool-tests', ['-L', 'tools'], '1')):
        require(selected[role]['argv'] == ['--preset', 'x86-64-debug',
                *arguments, '--output-on-failure'], role + ': suite selection')
        require(selected[role]['PONY_DEBUG'] == debug, role + ': codegen environment')
    for program, text in (('hello', 'Hello from Nix!'),
                          ('testprog', 'NixOS linking works!')):
        event = selected['compile-' + program]
        require(not event['existed'], program + ': stale executable accepted')
        output = Path(event['output'])
        require(output.is_relative_to(event['TMPDIR'])
                and output.name == program, program + ': private executable path')
        require(text in event['source'], program + ': generated source content')
        require(selected['inspect-interpreter']['argv'] == [
            '-l', selected['compile-hello']['output']], 'source interpreter selection')



def main():
    if os.name != 'posix':
        print('Nix source orchestration checks skipped: requires POSIX shell')
        return
    require(shutil.which('bash') is not None, 'bash is required')
    with tempfile.TemporaryDirectory(prefix='ponyc-nix-source space-') as temporary:
        root = Path(temporary)
        for directory in ('bin', 'work/build/debug', 'temporary/unrelated',
                          'toolchain/nix-support',
                          'work/.ci-scripts/nix', 'caller-home', 'caller-tmp',
                          'caller-temp', 'caller-tempdir'):
            (root / directory).mkdir(parents=True)
        (root / 'temporary/unrelated/keep').write_text('preserved')
        leaf = root / 'leaf.py'
        leaf.write_text('#!' + sys.executable + '\n' + LEAF)
        leaf.chmod(0o755)
        for command in ('cmake', 'ctest', 'readelf'):
            (root / 'bin' / command).symlink_to(leaf)
        (root / 'work/build/debug/ponyc').symlink_to(leaf)
        original = (HERE / 'nix-build-and-test.sh').read_text()
        require('cd /work' not in original, 'source script has no hardcoded /work')
        script = original
        result, events = run(root, script)
        require(result.returncode == 0, result.stdout + result.stderr)
        require('=== ALL TESTS PASSED ===' in result.stdout, 'source success marker')
        validate(root, events)
        for index, role in enumerate(ROLES):
            result, events = run(root, script, {role: 73})
            expected = (result.returncode != 0 if role == 'inspect-interpreter'
                        else result.returncode == 73)
            require(expected, role + ': failure status propagation')
            require(tuple(event['role'] for event in workload(events)) == ROLES[:index + 1],
                    role + ': workload continued after failure')
            require('=== ALL TESTS PASSED ===' not in result.stdout,
                    role + ': success marker after failure')
        for program in ('hello', 'testprog'):
            result, events = run(root, script, extra={'NO_OUTPUT': 'compile-' + program})
            require(result.returncode != 0 and ('/bin/' + program) in result.stderr,
                    program + ': successful no-op compiler accepted')
            require('=== ALL TESTS PASSED ===' not in result.stdout,
                    program + ': success without a fresh executable')
        result, events = run(root, script, extra={'INTERPRETER': '/lib64/ld-linux.so'})
        require(result.returncode == 1 and 'not in the Nix store' in result.stderr,
                'source non-Nix interpreter accepted')
        require('compile-testprog' not in [event['role'] for event in events],
                'source workload continued after interpreter rejection')
    print('Nix source orchestration, fresh programs and failure propagation passed')


if __name__ == '__main__':
    main()
