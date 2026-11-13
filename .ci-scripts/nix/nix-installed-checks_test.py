#!/usr/bin/env python3

import contextlib
import importlib.util
import io
import json
import os
import pathlib
import shutil
import signal
import sys
import tempfile
import time
from unittest import mock


path = pathlib.Path(__file__).with_name('nix-installed-checks.py')
specification = importlib.util.spec_from_file_location('installed_checks', path)
checks = importlib.util.module_from_spec(specification)
sys.modules[specification.name] = checks
specification.loader.exec_module(checks)


def require(condition, description):
    if not condition:
        raise AssertionError(description)


def require_reaped(process, description):
    try:
        os.waitpid(process.pid, os.WNOHANG)
    except ChildProcessError:
        return
    raise AssertionError(description)


def environment_and_commands(root):
    inherited = os.environ.copy()
    controlled = ('NIX_CC', 'NIX_CFLAGS_COMPILE', 'NIX_LDFLAGS', 'PONYPATH',
                  'LD_LIBRARY_PATH', 'LD_PRELOAD', 'CPATH', 'C_INCLUDE_PATH',
                  'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH', 'CFLAGS', 'CPPFLAGS',
                  'LDFLAGS', 'PYTHONPATH')
    expected = {'PATH': inherited.get('PATH', os.defpath), 'HOME': str(root),
                'LC_ALL': 'C', 'PYTHONDONTWRITEBYTECODE': '1'}
    try:
        for variable in controlled:
            os.environ.pop(variable, None)
        require(checks._environment(root) == expected,
                'absent inherited search variables changed child environment')
        for value in ('', '/deliberately-unusable-search-path'):
            for variable in controlled:
                os.environ[variable] = value
                require(checks._environment(root) == expected,
                        f'inherited {variable}={value!r} changed environment')
                os.environ.pop(variable)
            os.environ.update(dict.fromkeys(controlled, value))
            require(checks._environment(root) == expected,
                    f'combined inherited poison {value!r} changed environment')
            for variable in controlled:
                os.environ.pop(variable)
        for additions in (
            {'NIX_CC': '/selected/compiler'},
            {'NIX_CFLAGS_COMPILE': '-isystem /one -isystem /two'},
            {'NIX_LDFLAGS': '-L/one -L /two'},
            {'NIX_CC': '/selected/compiler',
             'NIX_CFLAGS_COMPILE': '-isystem /one -isystem /two',
             'NIX_LDFLAGS': '-L/one -L /two'},
        ):
            require(checks._environment(root, additions) == expected | additions,
                    f'deliberate additions changed: {additions}')
    finally:
        os.environ.clear()
        os.environ.update(inherited)
    arguments = ['', 'with spaces', 'quote"here', "single'quote", '$HOME',
                 '$(literal)', ';exit 73', '*', '\u00e9\u2603']
    result = checks._command(
        'argv probe', [sys.executable, '-B', '-c',
                       'import json,sys; print(json.dumps(sys.argv[1:]))',
                       *arguments], expected, root)
    require(not isinstance(result, checks._Failure), f'argv launch: {result}')
    require(result.code == 0, f'argv child exit: {result.code}')
    require(json.loads(result.stdout) == arguments, 'argument-index mismatch')
    result = checks._command(
        'failure probe', [sys.executable, '-B', '-c',
                          'import sys; print("stdout-witness"); '
                          'print("stderr-witness",file=sys.stderr); '
                          'sys.exit(73)'], expected, root)
    failure = checks._successful('failure probe', result)
    require(isinstance(failure, checks._Failure), 'child failure accepted')
    for text in ('failure probe', repr(sys.executable), 'exit 73',
                 "stdout: b'stdout-witness\\n'",
                 "stderr: b'stderr-witness\\n'"):
        require(text in str(failure), f'missing failure evidence: {text}')
    missing = root / 'absent-command'
    result = checks._command('missing command', [missing], expected, root)
    require(isinstance(result, checks._Failure)
            and 'missing command' in str(result) and str(missing) in str(result),
            f'missing command diagnostic: {result}')


def frame_partitions():
    values = ({'id': 17, 'text': 'installed \u00e9\u2603'},
              {'id': 18, 'text': 'second message'})
    observations = 0
    for frames in (checks._LineFrames(), checks._LspFrames()):
        payload = b''.join(frames.encode(json.dumps(
            value, ensure_ascii=False).encode()) for value in values)
        require(len(payload) <= 512, 'framing transcript exceeds fixture bound')
        partitions = [[len(payload)], [1] * len(payload)]
        partitions.extend([split, len(payload) - split]
                          for split in range(1, len(payload)))
        for lengths in partitions:
            buffer = bytearray()
            decoded = []
            position = 0
            for length in lengths:
                buffer.extend(payload[position:position + length])
                position += length
                while True:
                    result = frames.extract(buffer)
                    if result is None:
                        break
                    require(not isinstance(result, checks._Failure),
                            f'framing failure at partition {lengths}: {result}')
                    decoded.append(json.loads(result.decode()))
            require(decoded == list(values), 'partition changed JSON values')
            require(not buffer, 'partition left unconsumed bytes')
            require(position == len(payload), 'partition omitted bytes')
            observations += 1
    return observations


def child_io(root):
    program = root / 'peer.py'
    value = {'id': 17, 'text': '\u00e9\u2603'}
    encoded = checks._LspFrames().encode(json.dumps(
        value, ensure_ascii=False).encode())
    program.write_text(
        'import os, sys\n'
        'sys.stdin.buffer.read(1)\n'
        'os.write(2, b"x" * (256 * 1024))\n'
        f'os.write(1, {encoded!r})\n'
        'sys.stdin.buffer.read()\n')
    environment = checks._environment(root)
    peer = checks._Peer.start([sys.executable, '-B', program], environment,
                               root, checks._LspFrames())
    require(not isinstance(peer, checks._Failure), f'child startup: {peer}')
    process = peer.process
    try:
        failure = peer.send({'start': True}, timeout=5)
        require(failure is None, f'child send: {failure}')
        result = peer.receive(time.monotonic() + 5)
        require(result == value, f'UTF-8 response/stderr drainage: {result}')
        require(len(peer.stderr) == 4096, 'stderr diagnostic retention')
        failure = peer.finish(timeout=5)
        require(failure is None, f'EOF shutdown: {failure}')
    finally:
        peer.close()
    require_reaped(process, 'successful child was not already reaped')
    require(process.poll() == 0, 'successful child was not reaped')
    require(all(stream.closed for stream in (
        process.stdin, process.stdout, process.stderr)), 'streams remain open')

    program.write_text('import time\ntime.sleep(60)\n')
    peer = checks._Peer.start([sys.executable, '-B', program], environment,
                               root, checks._LineFrames())
    require(not isinstance(peer, checks._Failure), f'stall startup: {peer}')
    process = peer.process
    started = time.monotonic()
    try:
        result = peer.receive(time.monotonic() + 0.1)
        require(isinstance(result, checks._Failure)
                and 'deadline exceeded' in result.detail,
                f'stalled child deadline: {result}')
    finally:
        peer.close()
    require_reaped(process, 'stalled child was not already reaped')
    require(time.monotonic() - started < 5, 'stalled child cleanup exceeded bound')
    require(process.poll() is not None, 'stalled child was not reaped')


def read_partition_io(root):
    program = root / 'partition-peer.py'
    count = 0
    for frames in (checks._LineFrames(), checks._LspFrames()):
        if isinstance(frames, checks._LineFrames):
            values = ({'id': 17, 'module_path': '/fixture/\u00e9', 'entries': [
                {'name': 'Probe', 'kind': 'primitive'}]},
                {'id': 18, 'module_path': '/next', 'entries': []})
        else:
            values = ({'id': 17, 'result': [{'uri': 'file:///fixture/\u00e9',
                       'range': {'start': {'line': 2, 'character': 3},
                                 'end': {'line': 2, 'character': 8}}}]},
                      {'id': 18, 'result': None})
        payload = b''.join(frames.encode(json.dumps(
            value, ensure_ascii=False).encode()) for value in values)
        require(len(payload) <= 512, 'read transcript exceeds fixture bound')
        program.write_text('import os, sys\n'
                           'sys.stdin.buffer.read(1)\n'
                           f'os.write(1, {payload!r})\n'
                           'sys.stdin.buffer.read()\n')
        partitions = [[len(payload)], [1] * len(payload)]
        partitions.extend([split, len(payload) - split]
                          for split in range(1, len(payload)))
        for lengths in partitions:
            peer = checks._Peer.start(
                [sys.executable, '-B', program], checks._environment(root),
                root, frames)
            require(not isinstance(peer, checks._Failure),
                    f'partition child startup: {peer}')
            observed = []
            stdout = peer.process.stdout.fileno()

            def read(descriptor, size):
                if descriptor == stdout and len(observed) < len(lengths):
                    size = min(size, lengths[len(observed)])
                data = os.read(descriptor, size)
                if descriptor == stdout and data:
                    observed.append(len(data))
                return data

            peer.read = read
            try:
                require(peer.send({'start': True}, timeout=5) is None,
                        'partition child start')
                for expected in values:
                    if isinstance(frames, checks._LspFrames):
                        result = checks._lsp_response(peer, expected['id'], 5)
                    else:
                        result = peer.receive(time.monotonic() + 5)
                    require(result == expected,
                            f'production read partition changed response: '
                            f'{lengths}: {result}')
                require(observed == lengths,
                        f'production read coverage mismatch: '
                        f'{observed} != {lengths}')
                require(peer.finish(timeout=5) is None,
                        'partition child did not exit cleanly')
            finally:
                peer.close()
            count += 1
    return count


def protocol_failures(root):
    program = root / 'protocol-peer.py'
    variants = (
        (checks._LineFrames(), b'not-json\n', 'protocol JSON', ''),
        (checks._LineFrames(), b'{"id":17', 'protocol', 'unexpected EOF'),
        (checks._LineFrames(), b'', 'protocol', 'unexpected EOF'),
        (checks._LspFrames(), b'Other: 1\r\n\r\nx', 'protocol',
         'Content-Length'),
        (checks._LspFrames(), b'Content-Length: 20\r\n\r\n{}',
         'protocol', 'unexpected EOF'),
        (checks._LspFrames(), b'Content-Length: 2\r\n\r\n[]',
         'protocol JSON', 'expected object'),
        (checks._LspFrames(), b'Content-Length: 999999999\r\n\r\n',
         'protocol', 'oversized Content-Length'),
    )
    for frames, payload, phase, diagnostic in variants:
        program.write_text(f'import os\nos.write(1, {payload!r})\n')
        peer = checks._Peer.start([sys.executable, '-B', program],
                                   checks._environment(root), root, frames)
        require(not isinstance(peer, checks._Failure), f'startup: {peer}')
        try:
            result = peer.receive(time.monotonic() + 5)
            require(isinstance(result, checks._Failure)
                    and result.phase == phase and diagnostic in result.detail,
                    f'malformed/EOF classification: {payload!r}: {result}')
        finally:
            peer.close()


def invalid_installation(root):
    prefix = (root / 'missing-installation').resolve()
    workdir = root / 'invalid-installation-scratch'
    workdir.mkdir()
    canary = workdir / 'keep.txt'
    canary.write_bytes(b'caller-owned\n')
    result = checks._run(prefix, workdir)
    require(isinstance(result, checks._Failure),
            f'invalid installation accepted: {result}')
    require(result.phase == 'installation',
            f'invalid installation phase: {result}')
    require('missing executable' in result.detail,
            f'invalid installation diagnostic: {result}')
    require(str(prefix / 'bin/ponyc') in result.detail,
            f'invalid installation executable path: {result}')
    require(canary.read_bytes() == b'caller-owned\n',
            'invalid installation changed caller file')
    require(list(workdir.iterdir()) == [canary],
            'invalid installation created scratch')
    require(not prefix.exists(), 'invalid installation prefix created')


def scratch_ownership(root):
    prefix = root / 'installation'
    prefix.mkdir()
    base = root / 'caller-scratch'
    base.mkdir()
    canary = base / 'keep.txt'
    canary.write_bytes(b'caller-owned\n')
    seed = base / 'hello-output'
    seed.mkdir()
    (seed / 'hello').write_bytes(b'stale executable\n')
    observed = []

    def compiler(_prefix, child, environment, _metadata):
        observed.append(child)
        require(environment['HOME'] == str(child / 'home'), 'private child HOME')
        require(not (child / 'hello-output').exists(), 'stale output reused')
        return None

    names = ('_additive_checks', '_lint_checks', '_documentation_check',
             '_dependency_check', '_agent_check', '_lsp_check', '_linker_check')
    with mock.patch.object(checks, '_installation', return_value={}):
        patches = [mock.patch.object(checks, name, return_value=None)
                   for name in names]
        for patch in patches:
            patch.start()
        try:
            with mock.patch.object(checks, '_compiler_checks',
                                   side_effect=compiler):
                result = checks._run(prefix, base)
                require(result is None, f'successful scratch run: {result}')
            require(len(observed) == 1 and not observed[0].exists(),
                    'successful owned child not removed')
            for _ in range(2):
                with mock.patch.object(checks, '_compiler_checks',
                                       return_value=checks._Failure(
                                           'controlled failure', 'witness')):
                    result = checks._run(prefix, base)
                require(isinstance(result, checks._Failure)
                        and 'retained scratch:' in result.detail,
                        f'failed scratch diagnostic: {result}')
            retained = list(base.glob('ponyc-*'))
            require(len(retained) == 2 and retained[0] != retained[1],
                    'failed invocations did not own distinct retained children')
            require(all((child / 'home').is_dir() for child in retained),
                    'failed child scratch was removed')
        finally:
            for patch in reversed(patches):
                patch.stop()
    require(canary.is_file() and canary.read_bytes() == b'caller-owned\n',
            'caller file changed')
    require((seed / 'hello').is_file()
            and (seed / 'hello').read_bytes() == b'stale executable\n',
            'supplied seed output changed')
    result = checks._scratch(prefix / 'child', prefix)
    require(isinstance(result, checks._Failure)
            and 'inside installation' in result.detail,
            f'workdir containment diagnostic: {result}')
    file_base = root / 'caller-file'
    file_base.write_bytes(b'keep\n')
    result = checks._scratch(file_base, prefix)
    require(isinstance(result, checks._Failure)
            and str(file_base) in result.detail, f'file workdir: {result}')
    require(file_base.read_bytes() == b'keep\n', 'file workdir modified')
    for location in (pathlib.Path(root.anchor) / 'checks.py',
                     pathlib.Path(root.anchor) / 'checks' / 'checks.py'):
        with mock.patch.object(checks, '__file__', str(location)):
            child = checks._scratch(base, prefix)
        require(isinstance(child, pathlib.Path) and child.parent == base,
                f'standalone helper scratch: {location}: {child}')
        shutil.rmtree(child)
    if os.name == 'posix':
        alias = root / 'installation-alias'
        alias.symlink_to(prefix, target_is_directory=True)
        result = checks._scratch(alias, prefix)
        require(isinstance(result, checks._Failure)
                and 'symlink' in result.detail, f'symlink workdir: {result}')
        result = checks._scratch(alias / 'child', prefix)
        require(isinstance(result, checks._Failure)
                and 'inside installation' in result.detail,
                f'aliased parent containment: {result}')


def semantic_children(root):
    prefix = root / 'controlled-tools'
    (prefix / 'bin').mkdir(parents=True)
    executable = prefix / 'bin/ponyc'
    executable.write_text(f'#!{sys.executable}\n')
    executable.chmod(0o755)
    fixture = checks._fixture(root, 'no-op', 'actor Main\n')
    result = checks._compile_run(prefix, root, checks._environment(root),
                                 fixture, b'expected\n')
    require(isinstance(result, checks._Failure)
            and 'produced no executable' in result.detail,
            f'exit-zero no-op accepted: {result}')
    executable.write_text(
        f'#!{sys.executable}\nimport pathlib,sys\n'
        'arguments = sys.argv\n'
        'output = pathlib.Path(arguments[arguments.index("-o") + 1])\n'
        'name = arguments[arguments.index("-b") + 1]\n'
        'result = output / name\n'
        f'result.write_text({("#!" + sys.executable + chr(10) + "print('wrong-value')" + chr(10))!r})\n'
        'result.chmod(0o755)\n')
    fixture = checks._fixture(root, 'wrong-value', 'actor Main\n')
    result = checks._compile_run(prefix, root, checks._environment(root),
                                 fixture, b'expected\n')
    require(isinstance(result, checks._Failure)
            and "output b'wrong-value\\n', expected b'expected\\n'" in result.detail,
            f'wrong semantic output accepted: {result}')

    for index, (code, output, suffix, detail) in enumerate((
        (42, 'expected', None, None),
        (0, 'expected', ' execution', 'exit 0, expected 42'),
        (42, 'wrong-value', '',
         "output b'wrong-value\\n', expected b'expected\\n'"),
    )):
        program = (f'#!{sys.executable}\nimport sys\nprint({output!r})\n'
                   f'sys.exit({code})\n')
        executable.write_text(
            f'#!{sys.executable}\nimport pathlib,sys\n'
            'arguments = sys.argv\n'
            'output = pathlib.Path(arguments[arguments.index("-o") + 1])\n'
            'name = arguments[arguments.index("-b") + 1]\n'
            'result = output / name\n'
            f'result.write_text({program!r})\n'
            'result.chmod(0o755)\n')
        fixture = checks._fixture(root, f'expected-exit-{index}', 'actor Main\n')
        result = checks._compile_run(prefix, root, checks._environment(root),
                                     fixture, b'expected\n', expected_exit=42)
        if detail is None:
            require(isinstance(result, pathlib.Path),
                    f'expected nonzero exit rejected: {result}')
        else:
            require(isinstance(result, checks._Failure)
                    and result.phase == fixture.name + suffix
                    and detail in result.detail,
                    f'exit/output dimension not rejected: {result}')


def ssl_definition_checks(root):
    prefix = root / 'ssl-tools'
    (prefix / 'bin').mkdir(parents=True)
    compiler = prefix / 'bin/ponyc'
    compiler.write_text(
        f'#!{sys.executable}\nimport pathlib,sys\n'
        'args = sys.argv\n'
        'output = pathlib.Path(args[args.index("-o") + 1]) / "ssl"\n'
        f'output.write_text({("#!" + sys.executable + chr(10) + "from pathlib import Path; Path('executed').touch(); print('installed-ssl')" + chr(10))!r})\n'
        'output.chmod(0o755)\n')
    compiler.chmod(0o755)
    command = checks._command
    for index, expected in enumerate(('-Dopenssl_3.0.x', '-Dopenssl_4.0.x',
                                       '-Dopenssl_1.1.x', '-Dlibressl')):
        wrong = '-Dopenssl_4.0.x' if expected != '-Dopenssl_4.0.x' else '-Dopenssl_3.0.x'
        variants = ([expected], [], [wrong], [expected, wrong],
                    [expected, expected])
        for case, definitions in enumerate(variants):
            directory = root / f'ssl-selection-{index}-{case}'
            directory.mkdir()

            def produce(phase, arguments, environment, cwd, timeout=120):
                if pathlib.Path(arguments[0]) == compiler:
                    arguments = [value for value in arguments if value != expected]
                    arguments[1:1] = definitions
                return command(phase, arguments, environment, cwd, timeout)

            with mock.patch.object(checks, '_command', side_effect=produce):
                result = checks._ssl_check(prefix, directory,
                                           checks._environment(directory),
                                           {'sslDefine': expected})
            require((directory / 'ssl-output/ssl').is_file(),
                    'SSL selection did not exercise compiler producer')
            if case == 0:
                require(result is None and (directory / 'executed').exists(),
                        f'correct SSL selection failed: {result}')
            else:
                require(isinstance(result, checks._Failure)
                        and 'SSL-definition selection mismatch' in result.detail,
                        f'incorrect SSL selection accepted: {definitions}: {result}')
                require(not (directory / 'executed').exists(),
                        'incorrect SSL selection executed generated program')


def protocol_correlation(root):
    program = root / 'correlation-peer.py'
    notification = {'jsonrpc': '2.0', 'method': 'window/logMessage',
                    'params': {'message': 'interleaved'}}
    matching = {'jsonrpc': '2.0', 'id': 17, 'result': {'uri': 'file:///fixture'}}
    variants = (
        ([notification, matching], None, 17),
        ([{'id': 99, 'result': None}, matching], 'response ID mismatch', 17),
        ([{'id': True, 'result': None}], 'response ID mismatch', 1),
        ([{'id': 17, 'error': {'code': 1}}], 'unsuccessful response', 17),
        ([{'id': 17}], 'unsuccessful response', 17),
    )
    for messages, diagnostic, identifier in variants:
        payload = b''.join(checks._LspFrames().encode(json.dumps(
            message).encode()) for message in messages)
        program.write_text('import os,sys\n'
                           f'os.write(1, {payload!r})\n'
                           'sys.stdin.buffer.read()\n')
        peer = checks._Peer.start([sys.executable, '-B', program],
                                   checks._environment(root), root,
                                   checks._LspFrames())
        require(not isinstance(peer, checks._Failure), f'startup: {peer}')
        try:
            result = checks._lsp_response(peer, identifier, 5)
            if diagnostic:
                require(isinstance(result, checks._Failure)
                        and diagnostic in result.detail,
                        f'LSP correlation classification: {result}')
            else:
                require(result == matching,
                        f'interleaved notification changed response: {result}')
        finally:
            peer.close()


def agent_semantics(root):
    prefix = root / 'controlled-agent'
    (prefix / 'bin').mkdir(parents=True)
    executable = prefix / 'bin/pony-agent-server'
    variants = (
        ('positive', None),
        ('readiness-error', 'compilation failed'),
        ('wrong-id', 'response ID mismatch'),
        ('wrong-module', 'module path mismatch'),
        ('wrong-kind', 'missing primitive export'),
        ('missing-export', 'missing primitive export'),
        ('no-response', 'deadline exceeded'),
        ('stall-after-eof', 'deadline exceeded'),
    )
    for variant, diagnostic in variants:
        directory = root / ('agent-case-' + variant)
        directory.mkdir()
        marker = directory / 'query-received'
        executable.write_text(
            f'#!{sys.executable}\nimport json,pathlib,sys,time\n'
            f'variant = {variant!r}\n'
            'ready = {"id":"", "status":"ready", '
            '"message":"compilation successful"}\n'
            'if variant == "readiness-error": ready["status"] = "error"\n'
            'print(json.dumps(ready), flush=True)\n'
            'query = json.loads(sys.stdin.readline())\n'
            f'pathlib.Path({str(marker)!r}).write_text("received")\n'
            'response = {"id":query["id"], "module_path":query["module"], '
            '"entries":[{"name":"InstalledAgentProbe", '
            '"kind":"primitive"}]}\n'
            'if variant == "wrong-id": response["id"] = 99\n'
            'if variant == "wrong-module": response["module_path"] = "/wrong"\n'
            'if variant == "wrong-kind": response["entries"][0]["kind"] = "class"\n'
            'if variant == "missing-export": response["entries"] = []\n'
            'if variant == "no-response": time.sleep(60)\n'
            'print(json.dumps(response), flush=True)\n'
            'sys.stdin.read()\n'
            'if variant == "stall-after-eof": time.sleep(60)\n')
        executable.chmod(0o755)
        children = []
        original = checks._Peer.start

        def start(*arguments, **keywords):
            peer = original(*arguments, **keywords)
            if not isinstance(peer, checks._Failure):
                children.append(peer.process)
            return peer

        with mock.patch.object(checks._Peer, 'start', side_effect=start):
            result = checks._agent_check(
                prefix, directory, checks._environment(root), timeout=1)
        if diagnostic:
            require(isinstance(result, checks._Failure)
                    and diagnostic in result.detail,
                    f'agent {variant} semantic classification: {result}')
        else:
            require(result is None, f'agent positive interaction: {result}')
        if variant in ('no-response', 'stall-after-eof'):
            require(marker.read_text() == 'received',
                    f'{variant} did not reach the intended protocol phase')
        require(len(children) == 1, f'agent {variant} child count')
        require_reaped(children[0], f'agent {variant} child not already reaped')
        require(children[0].poll() is not None,
                f'agent {variant} child not reaped')
        require(all(stream.closed for stream in (
            children[0].stdin, children[0].stdout, children[0].stderr)),
            f'agent {variant} streams remain open')


def termination_and_timeout(root):
    program = root / 'ignore-termination.py'
    program.write_text('import json,signal,time\n'
                       'signal.signal(signal.SIGTERM, signal.SIG_IGN)\n'
                       'print(json.dumps({"ready": True}), flush=True)\n'
                       'time.sleep(60)\n')
    peer = checks._Peer.start([sys.executable, '-B', program],
                               checks._environment(root), root,
                               checks._LineFrames())
    require(not isinstance(peer, checks._Failure), f'ignore startup: {peer}')
    process = peer.process
    try:
        result = peer.receive(time.monotonic() + 5)
        require(result == {'ready': True}, 'SIGTERM-ignore readiness')
    finally:
        started = time.monotonic()
        peer.close()
    require_reaped(process, 'ignoring child was not already reaped')
    require(time.monotonic() - started < 5, 'kill escalation exceeded deadline')
    require(process.returncode == -signal.SIGKILL,
            f'ignoring child was not killed/reaped: {process.returncode}')
    marker = root / 'timed-command-pid'
    result = checks._command(
        'timed command', [sys.executable, '-B', '-c',
                          'import os,pathlib,sys,time; '
                          'pathlib.Path(sys.argv[1]).write_text(str(os.getpid())); '
                          'print("timeout-stdout",flush=True); time.sleep(60)',
                          marker], checks._environment(root), root, timeout=1)
    require(isinstance(result, checks._Failure)
            and 'deadline exceeded' in result.detail
            and "stdout: b'timeout-stdout\\n'" in result.detail,
            f'command timeout diagnostic: {result}')
    process_id = int(marker.read_text())
    try:
        os.waitpid(process_id, os.WNOHANG)
    except ChildProcessError:
        pass
    else:
        raise AssertionError('timed command child was not already reaped')


def main():
    count = frame_partitions()
    with tempfile.TemporaryDirectory(prefix='ponyc-helper-test-') as temporary:
        root = pathlib.Path(temporary)
        environment_and_commands(root)
        invalid_installation(root)
        with contextlib.redirect_stdout(io.StringIO()):
            scratch_ownership(root)
        if os.name == 'posix':
            child_io(root)
            reads = read_partition_io(root)
            protocol_failures(root)
            protocol_correlation(root)
            semantic_children(root)
            ssl_definition_checks(root)
            agent_semantics(root)
            termination_and_timeout(root)
            process_result = (f'{reads} production read partitions, child I/O '
                              'and deadline cleanup passed')
        else:
            process_result = 'POSIX pipe I/O skipped on this host'
    print(f'Installed checks: {count} framing partitions passed; {process_result}')


if __name__ == '__main__':
    main()
