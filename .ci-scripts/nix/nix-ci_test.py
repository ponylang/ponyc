#!/usr/bin/env python3

import json
import os
import pathlib
import re
import shlex
import shutil
import subprocess
import sys
import tempfile


REPO = pathlib.Path(__file__).resolve().parents[2]
HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(REPO / '.ci-scripts/libs-cache'))
from common import EXIT_ERROR, EXIT_MISS  # noqa: E402


STUB = '''import json,os,pathlib,subprocess,sys
name = pathlib.Path(sys.argv[0]).name
arguments = sys.argv[1:]
with open(os.environ['RECORD'], 'a') as record:
    record.write(json.dumps({'name':name, 'argv':arguments, 'cwd':os.getcwd()}) + '\\n')
results = json.loads(os.environ['LEAF_RESULTS'])
if name == 'sudo':
    code = results.get('sudo', 0)
    if code: sys.exit(code)
    assert arguments[0] == 'sysctl', arguments
    command = pathlib.Path(sys.argv[0]).parent / 'sysctl'
    sys.exit(subprocess.run([command, *arguments[1:]]).returncode)
if name == 'sysctl': sys.exit(results.get('sysctl', 0))
if name == 'nix':
    if '--command' in arguments:
        command = arguments[arguments.index('--command') + 1:]
        sys.exit(subprocess.run(command).returncode)
    action = 'package:check' if 'check' in arguments else 'package:build'
    sys.exit(results.get(action, 0))
if name == 'cmake': sys.exit(results.get('build', 0))
assert name == 'python3', name
if arguments[0] == '-B': arguments = arguments[1:]
if arguments[0] == '-c':
    assert arguments[1] == 'from common import EXIT_MISS; print(EXIT_MISS)'
    print(os.environ['MISS_CODE'])
    sys.exit(0)
script = pathlib.Path(arguments[0]).name
if script == 'oci_libs_cache.py': action = 'main:' + arguments[1]
elif script == 'branch_libs_cache.py': action = 'branch:' + arguments[1]
elif script == 'promote_libs_cache.py': action = 'promote'
elif script == 'resolve_libs_cache.py': action = 'restore'
else: raise AssertionError(arguments)
sys.exit(results.get(action, 0))
'''


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def job(filename, name):
    text = (REPO / '.github/workflows' / filename).read_text()
    matches = list(re.finditer(r'^  ' + re.escape(name)
                              + r':\n(?:(?!^  \S).|\n)*', text, re.MULTILINE))
    require(len(matches) == 1, f'{filename}: missing/ambiguous job {name}')
    return matches[0].group()


def step(block, name):
    matches = list(re.finditer(r'^      - name: ' + re.escape(name)
                              + r'\n(?:(?!^      - ).|\n)*', block,
                              re.MULTILINE))
    require(len(matches) == 1, f'missing/ambiguous step {name}')
    return matches[0].group()


def body(block):
    match = re.search(r'^        run: (.+)$', block, re.MULTILINE)
    require(match is not None, 'missing workflow run body')
    if match[1] != '|':
        return match[1] + '\n'
    lines = block[match.end():].splitlines()
    script = []
    for line in lines:
        if not line.strip():
            script.append('')
        elif line.startswith('          '):
            script.append(line[10:])
        else:
            break
    require(bool(script), 'empty workflow run body')
    return '\n'.join(script) + '\n'


def actions(events):
    selected = []
    for event in events:
        if event['name'] == 'cmake':
            selected.append('build')
        elif event['name'] == 'python3':
            arguments = event['argv']
            if arguments[0] == '-B':
                arguments = arguments[1:]
            if arguments[0] == '-c':
                continue
            script = pathlib.Path(arguments[0]).name
            selected.append({
                'oci_libs_cache.py': 'main:' + arguments[1],
                'branch_libs_cache.py': 'branch:' + arguments[1],
                'promote_libs_cache.py': 'promote',
                'resolve_libs_cache.py': 'restore',
            }[script])
    return selected


def run(root, script, results=None, environment=None):
    record = root / 'record.jsonl'
    record.write_text('')
    output = root / 'output'
    output.write_text('')
    marker = root / 'work/.libs-cache-miss'
    marker.unlink(missing_ok=True)
    selected = dict(os.environ, PATH=str(root / 'bin') + os.pathsep
                    + os.environ['PATH'], RECORD=str(record),
                    LEAF_RESULTS=json.dumps(results or {}),
                    MISS_CODE=str(EXIT_MISS),
                    GITHUB_TOKEN='controlled-token', GITHUB_REPOSITORY='probe/repo',
                    GITHUB_ACTOR='probe', GITHUB_OUTPUT=str(output),
                    GITHUB_WORKSPACE=str(root / 'work'), LIBS_TAG='controlled-tag',
                    EVENT_NAME='schedule', REF='main')
    selected.update(environment or {})
    result = subprocess.run(['bash', '-e', '-c', script], cwd=root / 'work',
                            env=selected, capture_output=True, text=True,
                            timeout=15)
    events = [json.loads(line) for line in record.read_text().splitlines()]
    return result, events, output.read_text(), marker.exists()


def setup(root):
    (root / 'bin').mkdir()
    (root / 'work/.ci-scripts/libs-cache').mkdir(parents=True)
    stub = root / 'stub.py'
    stub.write_text(f'#!{sys.executable}\n' + STUB)
    stub.chmod(0o755)
    for name in ('nix', 'cmake', 'python3', 'sudo', 'sysctl'):
        (root / 'bin' / name).symlink_to(stub)
    directory = root / 'work/.ci-scripts/nix'
    directory.mkdir()
    (directory / 'nix-build-and-test.sh').write_text(
        'printf \'{"name":"source-workload","argv":[],"cwd":"%s"}\\n\' '
        '"$PWD" >> "$RECORD"\nexit "${SOURCE_STATUS:-0}"\n')


def helper_operations(root):
    tag = "tag with spaces;$(not-executed)'"
    for operation, expected_actions in (
        ('restore', ['restore']),
        ('build-push-branch', ['build', 'branch:push']),
        ('build-push-main', ['build', 'main:push']),
    ):
        script = shlex.join(['bash', str(HERE / 'nix-libs-cache.bash'), operation])
        result, events, _, _ = run(root, script, environment={'LIBS_TAG': tag})
        require(result.returncode == 0, f'{operation}: {result.stderr}')
        require(actions(events) == expected_actions, f'{operation}: actions')
        leaves = [event for event in events if event['name'] == 'python3']
        require(len(leaves) == 1 and leaves[0]['argv'][-2:] == ['--tag', tag],
                f'{operation}: tag argument boundary')
        require(all(event['cwd'] == str(root / 'work') for event in events
                    if event['name'] in ('cmake', 'python3')),
                f'{operation}: workload cwd')
        leaf = expected_actions[-1]
        for code in (EXIT_MISS, EXIT_ERROR, 73):
            result, events, _, _ = run(root, script, {leaf: code})
            expected = 0 if operation == 'build-push-branch' else code
            require(result.returncode == expected, f'{operation}: leaf status {code}')
            require(actions(events) == expected_actions, f'{operation}: leaf sequence')
            if operation == 'build-push-branch':
                require('::warning::nixos branch libs cache push failed' in result.stdout,
                        'branch publication warning')
        if operation != 'restore':
            result, events, _, _ = run(root, script, {'build': 73})
            require(result.returncode == 73 and actions(events) == ['build'],
                    f'{operation}: build failure propagation')


def workflow_policies(root):
    source = job('ponyc-tier3.yml', 'nixos-linux')
    warmer = job('update-lib-cache.yml', 'nixos-linux')
    script = body(step(source, 'Check libs cache'))
    variants = (
        ({'main:exists': 0}, {}, 0, ['main:exists'], 'hit=true', False),
        ({'main:exists': EXIT_MISS}, {}, 0, ['main:exists'], '', True),
        ({'main:exists': EXIT_ERROR}, {}, EXIT_ERROR, ['main:exists'], '', False),
    )
    for results, environment, code, expected_actions, expected_output, marker in variants:
        result, events, output, marked = run(root, script, results, environment)
        require(result.returncode == code and actions(events) == expected_actions,
                f'source main lookup policy: {results}')
        require(output.strip() == expected_output and marked == marker,
                f'source main lookup outputs: {results}')
        if marker:
            require('skipping this tier-3 run' in result.stdout, 'explicit main skip')
    for branch, code, output in ((0, 0, 'hit=true'),
                                  (EXIT_MISS, 0, 'hit=false'),
                                  (EXIT_ERROR, EXIT_ERROR, '')):
        result, events, actual, marked = run(
            root, script, {'main:exists': EXIT_MISS, 'branch:exists': branch},
            {'EVENT_NAME': 'workflow_dispatch', 'REF': 'controlled-branch'})
        require(result.returncode == code and actual.strip() == output and not marked,
                f'off-main lookup status/skip: {branch}')
        require(actions(events) == ['main:exists', 'branch:exists'],
                'off-main branch lookup selection')
    script = body(step(warmer, 'Check libs cache'))
    for status in (0, EXIT_MISS, EXIT_ERROR):
        result, events, output, marked = run(root, script, {'main:exists': status})
        require(result.returncode == (EXIT_ERROR if status == EXIT_ERROR else 0),
                f'warmer strict main lookup: {status}')
        require(actions(events) == ['main:exists'] and not marked,
                'warmer main lookup action')
        expected = '' if status == EXIT_ERROR else ('hit=true' if status == 0 else 'hit=false')
        require(output.strip() == expected, 'warmer main lookup output')
    script = body(step(warmer, 'Promote branch libs if present'))
    for results, expected_actions, promoted, warning in (
        ({'branch:exists': 0}, ['branch:exists', 'promote'], True, False),
        ({'branch:exists': 0, 'promote': 73}, ['branch:exists', 'promote'], False, False),
        ({'branch:exists': EXIT_MISS}, ['branch:exists'], False, False),
        ({'branch:exists': EXIT_ERROR}, ['branch:exists'], False, True),
    ):
        result, events, output, marked = run(root, script, results)
        require(result.returncode == 0 and actions(events) == expected_actions and not marked,
                f'warmer branch/promotion policy: {results}')
        require(output.strip() == 'promoted=' + str(promoted).lower(),
                f'warmer promotion output: {results}')
        require(('::warning::' in result.stdout) == warning,
                f'warmer branch warning policy: {results}')


def nix_installation(root):
    expected_action = 'cachix/install-nix-action@3dc140bfc76c49fbd4dd958429132e3746bcb086'
    source = job('ponyc-tier3.yml', 'nixos-linux')
    warmer = job('update-lib-cache.yml', 'nixos-linux')
    package = job('nix-package.yml', 'package')
    for label, block in (('source', source), ('warmer', warmer),
                         ('package', package)):
        install = step(block, 'Install Nix')
        require(expected_action in install, f'{label}: install action pin')
        require('sandbox = true' in install and 'sandbox-fallback = false' in install,
                f'{label}: strict sandbox in install config')
        require('docker' not in block.lower(), f'{label}: no Docker references')


def workflow_entries(root):
    source = job('ponyc-tier3.yml', 'nixos-linux')
    package = job('nix-package.yml', 'package')
    script = body(step(source, 'Build and test'))
    for status in (0, 73):
        result, events, _, _ = run(root, script, environment={'SOURCE_STATUS': str(status)})
        require(result.returncode == status, 'source workload status propagation')
        entries = [event for event in events if event['name'] == 'nix']
        require(len(entries) == 1, 'source locked shell selection')
        require(entries[0]['cwd'] == str(root / 'work'), 'source shell entry cwd')
        require(entries[0]['argv'] == [
            'develop', '--no-update-lock-file', '--command', 'bash',
            '.ci-scripts/nix/nix-build-and-test.sh'], 'source locked shell entry')
        workloads = [event for event in events if event['name'] == 'source-workload']
        require(len(workloads) == 1 and workloads[0]['cwd'] == str(root / 'work'),
                'source workload cwd')
    for step_name, mode in (('Build package', 'build'),
                            ('Check installed tools', 'check')):
        script = body(step(package, step_name))
        for status in (0, 73):
            result, events, _, _ = run(root, script, {'package:' + mode: status})
            require(result.returncode == status, f'package {mode}: status propagation')
            entries = [event for event in events if event['name'] == 'nix']
            require(len(entries) == 1, f'package {mode}: locked operation selection')
            require(entries[0]['cwd'] == str(root / 'work'), f'package {mode}: cwd')
            arguments = entries[0]['argv']
            require('--no-update-lock-file' in arguments,
                    f'package {mode}: strict lock entry')
            require((mode == 'build' and 'build' in arguments and '.#ponyc' in arguments)
                    or (mode == 'check' and 'flake' in arguments and 'check' in arguments),
                    f'package {mode}: operation selection')


def sandbox_namespaces(root, filename, job_name):
    label = f'{filename}/{job_name}'
    block = job(filename, job_name)
    preparation = step(block, 'Enable Nix sandbox namespaces')
    install = step(block, 'Install Nix')
    headers = list(re.finditer(r'^      - .+$', block, re.MULTILINE))
    positions = {match.start(): index for index, match in enumerate(headers)}
    require(positions[block.index(preparation)] + 1
            == positions[block.index(install)], f'{label}: preparation adjacency')
    expected = {
        ('nix-package.yml', 'package'): [],
        ('ponyc-tier3.yml', 'nixos-linux'):
            ["${{ hashFiles('.libs-cache-miss') == '' }}"],
        ('update-lib-cache.yml', 'nixos-linux'):
            ["steps.check.outputs.hit != 'true' "
             "&& steps.promote.outputs.promoted != 'true'"],
    }[(filename, job_name)]
    for name, selected in (('preparation', preparation), ('install', install)):
        conditions = re.findall(r'^        if: (.+)$', selected, re.MULTILINE)
        require(conditions == expected, f'{label}: {name} execution condition')
        require(re.search(r'^        continue-on-error:', selected,
                          re.MULTILINE) is None,
                f'{label}: {name} failure tolerance')
    require(re.search(r'^        shell:', preparation, re.MULTILINE) is None,
            f'{label}: preparation shell override')
    arguments = ['-w', 'kernel.apparmor_restrict_unprivileged_userns=0']
    for boundary, code in (('success', 0), ('sysctl', 1), ('sysctl', 73),
                           ('sudo', 1), ('sudo', 73)):
        results = {'sudo': 0, 'sysctl': 0}
        if boundary != 'success':
            results[boundary] = code
        result, events, _, _ = run(root, body(preparation), results)
        case = f'{label}: {boundary} status {code}'
        require(result.returncode == code, f'{case}: status propagation')
        names = ['sudo'] if boundary == 'sudo' else ['sudo', 'sysctl']
        require([event['name'] for event in events] == names,
                f'{case}: invocation sequence')
        require(events[0]['argv'] == ['sysctl', *arguments],
                f'{case}: sudo argument vector')
        if boundary != 'sudo':
            require(events[1]['argv'] == arguments,
                    f'{case}: sysctl argument vector')


def identities_and_authority():
    identities = []
    for filename in ('ponyc-tier3.yml', 'update-lib-cache.yml'):
        block = job(filename, 'nixos-linux')
        expressions = re.findall(r'LIBS_TAG: \$\{\{ hashFiles\(([^\n]+)\) \}\}', block)
        require(len(expressions) == 3, f'{filename}: cache identity producer count')
        identities.extend(expressions)
        text = (REPO / '.github/workflows' / filename).read_text()
        require('flake.nix' not in text.replace(block, ''),
                f'{filename}: Nix input leaked into other platform identities')
    require(len(set(identities)) == 1, 'Nix cache identity parity')
    text = (REPO / '.github/workflows/nix-package.yml').read_text()
    permissions = re.search(r'^permissions:\n((?:  .+\n)+)', text, re.MULTILINE)
    require(permissions is not None and permissions[1] == '  contents: read\n',
            'package workflow read-only permission boundary')
    require('secrets.' not in text and 'pull_request_target' not in text,
            'package workflow credential/event boundary')
    require('persist-credentials: false' in text,
            'package checkout persisted credentials')
    require('pull_request' not in text,
            'package workflow must not trigger on pull_request')
    require('  workflow_dispatch:\n' in text and '  schedule:\n' in text
            and 'ref: ${{ inputs.ref || github.sha }}' in text,
            'package current-source scheduled/manual selection')


def main():
    if os.name != 'posix':
        print('Nix CI script execution skipped: requires POSIX shell and pipes')
        return
    require(shutil.which('bash') is not None, 'bash is required for Nix CI script tests')
    with tempfile.TemporaryDirectory(prefix='ponyc-nix-ci-test-') as temporary:
        root = pathlib.Path(temporary)
        setup(root)
        helper_operations(root)
        workflow_policies(root)
        nix_installation(root)
        workflow_entries(root)
        for filename, job_name in (('nix-package.yml', 'package'),
                                   ('ponyc-tier3.yml', 'nixos-linux'),
                                   ('update-lib-cache.yml', 'nixos-linux')):
            sandbox_namespaces(root, filename, job_name)
        identities_and_authority()
    print('Nix CI installation, locked entries, cache policies and authority passed')


if __name__ == '__main__':
    main()
