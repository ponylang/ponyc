#!/usr/bin/env python3

import argparse
import dataclasses
import json
import os
import pathlib
import re
import selectors
import shutil
import struct
import subprocess
import sys
import tempfile
import time


@dataclasses.dataclass(frozen=True)
class _Failure:
    phase: str
    detail: str

    def __str__(self):
        return f'{self.phase}: {self.detail}'


@dataclasses.dataclass(frozen=True)
class _Result:
    code: int
    stdout: bytes
    stderr: bytes
    command: tuple = ()


class _LineFrames:
    def extract(self, buffer):
        position = buffer.find(b'\n')
        if position < 0:
            return None
        payload = bytes(buffer[:position])
        del buffer[:position + 1]
        return payload

    def encode(self, payload):
        return payload + b'\n'


class _LspFrames:
    def extract(self, buffer):
        position = buffer.find(b'\r\n\r\n')
        if position < 0:
            return None
        lengths = re.findall(rb'(?:^|\r\n)Content-Length: ([0-9]+)(?=\r\n|$)',
                             bytes(buffer[:position]), re.IGNORECASE)
        if len(lengths) != 1:
            return _Failure('protocol', 'missing unique Content-Length')
        if len(lengths[0]) > 8:
            return _Failure('protocol', 'oversized Content-Length')
        length = int(lengths[0])
        if length > 1024 * 1024:
            return _Failure('protocol', 'oversized message')
        start = position + 4
        if len(buffer) < start + length:
            return None
        payload = bytes(buffer[start:start + length])
        del buffer[:start + length]
        return payload

    def encode(self, payload):
        return f'Content-Length: {len(payload)}\r\n\r\n'.encode() + payload


class _Peer:
    def __init__(self, process, frames, read=os.read):
        self.process = process
        self.frames = frames
        self.read = read
        self.buffer = bytearray()
        self.stderr = bytearray()
        self.outgoing = bytearray()
        self.eof = False
        self.selector = selectors.DefaultSelector()
        for stream in (process.stdin, process.stdout, process.stderr):
            os.set_blocking(stream.fileno(), False)
        self.selector.register(process.stdout, selectors.EVENT_READ, 'stdout')
        self.selector.register(process.stderr, selectors.EVENT_READ, 'stderr')

    @classmethod
    def start(cls, arguments, environment, directory, frames):
        try:
            process = subprocess.Popen(
                [str(argument) for argument in arguments], env=environment,
                cwd=directory, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                stderr=subprocess.PIPE, bufsize=0)
        except OSError as error:
            return _Failure('protocol startup', str(error))
        try:
            return cls(process, frames)
        except (OSError, ValueError) as error:
            process.kill()
            process.wait()
            for stream in (process.stdin, process.stdout, process.stderr):
                stream.close()
            return _Failure('protocol startup', str(error))

    def _pump(self, deadline):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return _Failure('protocol', 'deadline exceeded; stderr: '
                            + self.stderr.decode(errors='replace'))
        events = self.selector.select(remaining)
        if not events:
            return _Failure('protocol', 'deadline exceeded; stderr: '
                            + self.stderr.decode(errors='replace'))
        for key, _ in events:
            try:
                if key.data == 'stdin':
                    written = os.write(key.fd, self.outgoing)
                    del self.outgoing[:written]
                    if not self.outgoing:
                        self.selector.unregister(key.fileobj)
                else:
                    data = self.read(key.fd, 65536)
                    if not data:
                        self.selector.unregister(key.fileobj)
                        if key.data == 'stdout':
                            self.eof = True
                    elif key.data == 'stdout':
                        self.buffer.extend(data)
                        if len(self.buffer) > 1024 * 1024:
                            return _Failure('protocol', 'oversized stdout')
                    else:
                        self.stderr.extend(data)
                        del self.stderr[:-4096]
            except BlockingIOError:
                continue
            except OSError as error:
                return _Failure('protocol I/O', str(error))
        return None

    def send(self, message, timeout=120):
        payload = json.dumps(message, ensure_ascii=False).encode('utf-8')
        self.outgoing.extend(self.frames.encode(payload))
        try:
            self.selector.register(self.process.stdin, selectors.EVENT_WRITE,
                                   'stdin')
        except (KeyError, ValueError) as error:
            return _Failure('protocol send', str(error))
        deadline = time.monotonic() + timeout
        while self.outgoing:
            failure = self._pump(deadline)
            if failure:
                return failure
        return None

    def receive(self, deadline):
        while True:
            payload = self.frames.extract(self.buffer)
            if isinstance(payload, _Failure):
                return payload
            if payload is not None:
                try:
                    value = json.loads(payload.decode('utf-8'))
                except (UnicodeError, json.JSONDecodeError) as error:
                    return _Failure('protocol JSON', str(error))
                if not isinstance(value, dict):
                    return _Failure('protocol JSON', 'expected object')
                return value
            if self.eof:
                return _Failure('protocol', 'unexpected EOF; stderr: '
                                + self.stderr.decode(errors='replace'))
            failure = self._pump(deadline)
            if failure:
                return failure

    def finish(self, timeout=15):
        self.process.stdin.close()
        deadline = time.monotonic() + timeout
        while self.selector.get_map():
            failure = self._pump(deadline)
            if failure:
                return failure
        try:
            code = self.process.wait(timeout=max(0.001,
                                                deadline - time.monotonic()))
        except subprocess.TimeoutExpired:
            return _Failure('protocol shutdown', 'deadline exceeded')
        if code:
            return _Failure('protocol shutdown', f'exit {code}; stderr: '
                            + self.stderr.decode(errors='replace'))
        return None

    def close(self):
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        self.selector.close()
        for stream in (self.process.stdin, self.process.stdout,
                       self.process.stderr):
            stream.close()


def _environment(home, additions=None):
    environment = {
        'PATH': os.environ.get('PATH', os.defpath),
        'HOME': str(home),
        'LC_ALL': 'C',
        'PYTHONDONTWRITEBYTECODE': '1',
    }
    if additions:
        environment.update(additions)
    return environment


def _command(phase, arguments, environment, directory, timeout=120):
    command = tuple(str(argument) for argument in arguments)
    try:
        result = subprocess.run(
            command, cwd=directory,
            env=environment, stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        return _Failure(phase, f'command deadline exceeded: {command!r}; '
                        f'stdout: {(error.stdout or b"")[-4096:]!r}; '
                        f'stderr: {(error.stderr or b"")[-4096:]!r}')
    except OSError as error:
        return _Failure(phase, f'cannot execute {arguments[0]}: {error}')
    return _Result(result.returncode, result.stdout, result.stderr, command)


def _successful(phase, result, expected=0):
    if isinstance(result, _Failure):
        return result
    if result.code != expected:
        return _Failure(phase, f'command {result.command!r}; exit '
                        f'{result.code}, expected {expected}; '
                        f'stdout: {result.stdout[-4096:]!r}; '
                        f'stderr: {result.stderr[-4096:]!r}')
    return None


def _installation(prefix):
    for name in ('ponyc', 'pony-lsp', 'pony-lint', 'pony-doc', 'pony-dep',
                 'pony-agent-server'):
        executable = prefix / 'bin' / name
        if not executable.resolve().is_relative_to(prefix):
            return _Failure('installation', f'{name} resolves outside prefix')
        if not executable.is_file() or not os.access(executable, os.X_OK):
            return _Failure('installation', f'missing executable: {executable}')
    metadata = prefix / 'nix-support/installed-toolchain.json'
    try:
        value = json.loads(metadata.read_text())
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        return _Failure('installation', f'cannot read {metadata}: {error}')
    fields = ('version', 'opensslVersion', 'cmakeSslDefine', 'compiler',
              'ar', 'interpreter')
    if not isinstance(value, dict) or any(
            not isinstance(value.get(field), str) or not value[field]
            for field in fields):
        return _Failure('installation', 'invalid toolchain metadata')
    for field in ('includeDirectories', 'libraryDirectories'):
        entries = value.get(field)
        if not isinstance(entries, list) or not (1 <= len(entries) < 63) or any(
                not isinstance(entry, str) or not entry.startswith('/')
                or any(character.isspace() for character in entry)
                for entry in entries):
            return _Failure('installation', f'invalid {field} baseline')
    version = tuple(int(part) for part in value['opensslVersion'].split('.')
                    if part.isdigit())
    if version >= (4, 0):
        define = '-Dopenssl_4.0.x'
    elif version >= (3, 0):
        define = '-Dopenssl_3.0.x'
    elif version >= (1, 1):
        define = '-Dopenssl_1.1.x'
    else:
        return _Failure('installation', 'unsupported pinned OpenSSL version')
    if value['cmakeSslDefine'] != define:
        return _Failure('installation', 'SSL definition disagrees with pinned '
                        'OpenSSL version')
    value['sslDefine'] = define
    return value


def _scratch(base, prefix):
    if base.is_symlink():
        return _Failure('scratch', f'workdir is a symlink: {base}')
    resolved = base.resolve()
    candidate = pathlib.Path(__file__).resolve().parent.parent.parent
    source = candidate if (candidate / 'CMakeLists.txt').is_file() else None
    if resolved.is_relative_to(prefix):
        return _Failure('scratch', 'workdir is inside installation')
    if source is not None and resolved.is_relative_to(source):
        return _Failure('scratch', 'workdir is inside source tree')
    try:
        resolved.mkdir(parents=True, exist_ok=True)
        child = pathlib.Path(tempfile.mkdtemp(prefix='ponyc-', dir=resolved))
        (child / 'home').mkdir()
    except OSError as error:
        return _Failure('scratch', str(error))
    return child


def _fixture(root, name, pony, c=None):
    directory = root / name
    directory.mkdir()
    (directory / 'main.pony').write_text(pony)
    if c is not None:
        (directory / 'shim.c').write_text(c)
    return directory


def _compile_fixture(prefix, root, environment, fixture, flags=()):
    phase = fixture.name
    output = root / (fixture.name + '-output')
    output.mkdir()
    executable = output / fixture.name
    if executable.exists():
        return _Failure(phase, 'executable already exists before compilation')
    result = _command(phase, [prefix / 'bin/ponyc', *flags, '-b', fixture.name,
                             '-o', output, fixture], environment, root)
    return executable, result


def _compile_run(prefix, root, environment, fixture, expected, flags=(),
                 warning=None, expected_exit=0):
    phase = fixture.name
    compilation = _compile_fixture(prefix, root, environment, fixture, flags)
    if isinstance(compilation, _Failure):
        return compilation
    executable, result = compilation
    failure = _successful(phase + ' compilation', result)
    if failure:
        return failure
    if warning and warning not in result.stdout + result.stderr:
        return _Failure(phase, f'missing overflow warning: {warning!r}')
    return _execute_fixture(phase, executable, expected, environment, root,
                            expected_exit)


def _execute_fixture(phase, executable, expected, environment, root,
                     expected_exit=0):
    if not executable.is_file():
        return _Failure(phase, 'compiler produced no executable')
    result = _command(phase, [executable, '--ponymaxthreads=2'], environment,
                      root)
    failure = _successful(phase + ' execution', result, expected_exit)
    if failure:
        return failure
    if result.stdout != expected:
        return _Failure(phase, f'output {result.stdout!r}, expected {expected!r}')
    return executable


def _elf_check(executable, interpreter, environment, root):
    result = _command('ELF inspection', ['readelf', '-l', '-d', executable],
                      environment, root)
    failure = _successful('ELF inspection', result)
    if failure:
        return failure
    text = result.stdout.decode(errors='replace')
    actual = re.findall(r'Requesting program interpreter: ([^\]]+)', text)
    if actual != [interpreter]:
        return _Failure('ELF inspection', f'interpreter {actual!r}, expected '
                        f'{interpreter!r}')
    paths = re.findall(r'\((?:RPATH|RUNPATH)\).*?\[([^\]]*)\]', text)
    for directories in paths:
        if any(not entry.startswith(('/nix/store/', '$ORIGIN'))
               for entry in directories.split(':')):
            return _Failure('ELF inspection', f'unexpected runtime path: '
                            f'{directories}')
    result = _command('ELF dependencies', [interpreter, '--list', executable],
                      environment, root)
    failure = _successful('ELF dependencies', result)
    if failure:
        return failure
    if b'not found' in result.stdout or b'not found' in result.stderr:
        return _Failure('ELF dependencies', 'unresolved runtime dependency')
    return None


def _compiler_checks(prefix, root, environment, metadata):
    main = ('actor Main\n  new create(env: Env) =>\n'
            '    env.out.print("installed-hello")\n')
    hello = _fixture(root, 'hello', main)
    result = _compile_run(prefix, root, environment, hello,
                          b'installed-hello\n')
    if isinstance(result, _Failure):
        return result
    failure = _elf_check(result, metadata['interpreter'], environment, root)
    if failure:
        return failure
    shim_main = ('use @installed_probe[I32]()\n\nactor Main\n'
                 '  new create(env: Env) =>\n'
                 '    let result = @installed_probe()\n'
                 '    env.out.print(result.string())\n')
    probes = (
        ('system-header', '#include <sys/utsname.h>\n'
         'int installed_probe(void) {\n  struct utsname value;\n'
         '  return uname(&value) == 0 ? 42 : 1;\n}\n', shim_main, 0),
        ('pony-header', '#include <pony.h>\n#include <ponyassert.h>\n'
         'int installed_probe(void) {\n'
         '  pony_assert(pony_ctx() != 0);\n  return 42;\n}\n',
         shim_main + '    env.exitcode(result)\n', 42),
    )
    for name, c, pony, expected_exit in probes:
        fixture = _fixture(root, name, pony, c)
        result = _compile_run(prefix, root, environment, fixture, b'42\n',
                              expected_exit=expected_exit)
        if isinstance(result, _Failure):
            return result
    hostile = dict(environment, NIX_CC='/deliberately-unusable-nix-cc')
    fixture = _fixture(root, 'hostile-cc', main)
    result = _compile_run(prefix, root, hostile, fixture, b'installed-hello\n')
    if isinstance(result, _Failure):
        return result
    return _ssl_check(prefix, root, environment, metadata)


def _ssl_check(prefix, root, environment, metadata):
    ssl = _fixture(root, 'ssl', '''use "net"

actor Main
  new create(env: Env) =>
    try
      let context = recover val
        let mutable = SSLContext
        mutable.set_min_proto_version(TLS1u3Version())?
        if mutable.get_min_proto_version() != TLS1u3Version().ilong() then
          error
        end
        mutable
      end
      let session = context.client()?
      session.dispose()
      env.out.print("installed-ssl")
    else
      env.exitcode(1)
    end
''')
    compilation = _compile_fixture(prefix, root, environment, ssl,
                                   [metadata['sslDefine']])
    if isinstance(compilation, _Failure):
        return compilation
    executable, result = compilation
    if isinstance(result, _Failure):
        return result
    definitions = [value for value in result.command
                   if value.startswith(('-Dopenssl_', '-Dlibressl'))]
    if definitions != [metadata['sslDefine']]:
        return _Failure('ssl', f'SSL-definition selection mismatch: '
                        f'{definitions!r}, expected {[metadata["sslDefine"]]!r}')
    failure = _successful('ssl compilation', result)
    if failure:
        return failure
    result = _execute_fixture('ssl', executable, b'installed-ssl\n', environment,
                              root)
    return result if isinstance(result, _Failure) else None


def _excluded_dependency(prefix, root, environment, fixture, warning,
                         diagnostic):
    compilation = _compile_fixture(prefix, root, environment, fixture)
    if isinstance(compilation, _Failure):
        return compilation
    executable, result = compilation
    if isinstance(result, _Failure):
        return result
    text = result.stdout + result.stderr
    if result.code == 0 or executable.exists():
        return _Failure(fixture.name, 'excluded dependency unexpectedly resolved')
    if warning not in text:
        return _Failure(fixture.name, f'missing overflow warning: {warning!r}')
    if re.search(diagnostic, text) is None:
        return _Failure(fixture.name, f'missing dependency diagnostic: {text!r}')
    return None


def _additive_checks(prefix, root, environment, metadata):
    with tempfile.TemporaryDirectory(prefix='ponyc-flag-inputs-',
                                     dir='/tmp') as temporary:
        inputs = pathlib.Path(temporary)
        include = inputs / 'include'
        library = inputs / 'library'
        include.mkdir()
        library.mkdir()
        (include / 'installed-private.h').write_text(
            '#define INSTALLED_PRIVATE 19\n')
        implementation = inputs / 'private.c'
        implementation.write_text('int installed_private(void) { return 73; }\n')
        object_file = inputs / 'private.o'
        archive = library / 'libinstalled_private.a'
        commands = (
            [pathlib.Path(metadata['compiler']) / 'bin/cc', '-c', implementation,
             '-o', object_file],
            [metadata['ar'], 'rcs', archive, object_file],
        )
        for command in commands:
            failure = _successful('private library setup', _command(
                'private library setup', command, environment, inputs))
            if failure:
                return failure
        if not object_file.is_file() or not archive.is_file():
            return _Failure('private library setup', 'missing prebuilt archive')
        header_main = ('use @installed_header[I32]()\n\nactor Main\n'
                       '  new create(env: Env) =>\n'
                       '    env.out.print(@installed_header().string())\n')
        header_shim = ('#include <sys/utsname.h>\n'
                       '#include <installed-private.h>\n'
                       'int installed_header(void) {\n'
                       '  return sizeof(struct utsname) > 0 ? '
                       'INSTALLED_PRIVATE : 0;\n}\n')
        library_main = ('use "lib:installed_private"\n'
                        'use @installed_private[I32]()\n\nactor Main\n'
                        '  new create(env: Env) =>\n'
                        '    env.out.print(@installed_private().string())\n')
        hello = ('actor Main\n  new create(env: Env) =>\n'
                 '    env.out.print("capacity-baseline")\n')
        empty = []
        for index in range(64):
            directory = inputs / f'empty-{index}'
            directory.mkdir()
            empty.append(directory)
        variants = (
            ('include', 'NIX_CFLAGS_COMPILE', 'includeDirectories',
             lambda directory: f'-isystem {directory}', include,
             header_main, header_shim, b'19\n',
             b'NIX_CFLAGS_COMPILE has more than 64 -isystem entries',
             rb"installed-private[.]h[^\n]*(?:file not found|No such file)"),
            ('library', 'NIX_LDFLAGS', 'libraryDirectories',
             lambda directory: f'-L{directory}', library,
             library_main, None, b'73\n',
             b'NIX_LDFLAGS has more than 64 -L entries',
             rb'unable to find library -linstalled_private'),
        )
        for (kind, variable, baseline_field, flag, sentinel, pony, c, expected,
             warning, diagnostic) in variants:
            allowance = 64 - len(metadata[baseline_field])
            print(f'{kind} caller allowance: {allowance}', flush=True)
            fixture = _fixture(root, f'{kind}-zero', hello)
            result = _compile_run(prefix, root, environment, fixture,
                                  b'capacity-baseline\n')
            if isinstance(result, _Failure):
                return result
            for count in (1, allowance - 1, allowance, allowance + 1):
                directories = [*empty[:count - 1], sentinel]
                addition = ' '.join(flag(directory) for directory in directories)
                selected = dict(environment, **{variable: addition})
                fixture = _fixture(root, f'{kind}-{count}', pony, c)
                if count <= allowance:
                    result = _compile_run(prefix, root, selected, fixture,
                                          expected)
                    if isinstance(result, _Failure):
                        return result
                else:
                    failure = _excluded_dependency(
                        prefix, root, selected, fixture, warning, diagnostic)
                    if failure:
                        return failure
                    baseline = _fixture(root, f'{kind}-overflow-baseline', hello)
                    result = _compile_run(prefix, root, selected, baseline,
                                          b'capacity-baseline\n',
                                          warning=warning)
                    if isinstance(result, _Failure):
                        return result
        additions = {
            'NIX_CFLAGS_COMPILE': ' '.join(
                f'-isystem {directory}' for directory in [
                    *empty[:63 - len(metadata['includeDirectories'])], include]),
            'NIX_LDFLAGS': ' '.join(
                f'-L {directory}' for directory in [
                    *empty[:63 - len(metadata['libraryDirectories'])], library]),
        }
        combined = _fixture(root, 'combined-additions', library_main)
        result = _compile_run(prefix, root, dict(environment, **additions),
                              combined, b'73\n')
        if isinstance(result, _Failure):
            return result
        combined = _fixture(root, 'combined-header', header_main, header_shim)
        result = _compile_run(prefix, root, dict(environment, **additions),
                              combined, b'19\n')
        return result if isinstance(result, _Failure) else None


def _archive_entries(data):
    if not data or data[0] != 1:
        return _Failure('dependency archive', 'invalid archive version')
    position = 1
    files = {}
    directories = set()
    while position < len(data):
        tag = data[position]
        position += 1
        if tag not in (1, 2) or position + 4 > len(data):
            return _Failure('dependency archive', 'invalid entry header')
        length = struct.unpack_from('<I', data, position)[0]
        position += 4
        if position + length > len(data):
            return _Failure('dependency archive', 'truncated entry name')
        try:
            name = data[position:position + length].decode('utf-8')
        except UnicodeDecodeError:
            return _Failure('dependency archive', 'invalid UTF-8 name')
        position += length
        if name in files or name in directories:
            return _Failure('dependency archive', 'duplicate entry name')
        if tag == 2:
            directories.add(name)
            continue
        if position + 4 > len(data):
            return _Failure('dependency archive', 'missing content length')
        length = struct.unpack_from('<I', data, position)[0]
        position += 4
        if position + length > len(data):
            return _Failure('dependency archive', 'truncated content')
        if name in files:
            return _Failure('dependency archive', 'duplicate file entry')
        files[name] = data[position:position + length]
        position += length
    return files, directories


def _lint_checks(prefix, root, environment):
    config = root / 'lint-config.json'
    config.write_text(json.dumps({'rules': {'style': 'off', 'safety': 'on'}}))
    complete = ('type Color is (Red | Green | Blue)\n'
                'primitive Red\nprimitive Green\nprimitive Blue\n\n'
                'primitive Foo\n  fun apply(c: Color): String =>\n'
                '    match c\n    | Red => "red"\n'
                '    | Green => "green"\n    | Blue => "blue"\n    end\n')
    variants = (
        ('lint-complete', complete, 1),
        ('lint-annotated', complete.replace('match c',
                                          'match \\exhaustive\\ c'), 0),
        ('lint-incomplete', complete.replace(': String =>',
                                            ': (String | None) =>').replace(
                                                '    | Blue => "blue"\n', ''),
         0),
    )
    for name, source, status in variants:
        fixture = root / name
        fixture.mkdir()
        (fixture / 'fixture.pony').write_text(source)
        result = _command(name, [prefix / 'bin/pony-lint', '--config', config,
                                 fixture], environment, root)
        failure = _successful(name, result, status)
        if failure:
            return failure
        text = (result.stdout + result.stderr).decode(errors='replace')
        diagnostics = re.findall(r'([^\n]+):([0-9]+):([0-9]+): '
                                 r'error\[([^\]]+)\]: ([^\n]+)', text)
        if status:
            expected = ('fixture.pony', '8', '5',
                        'safety/exhaustive-match',
                        'exhaustive match should use \\exhaustive\\ annotation')
            if len(diagnostics) != 1 or (
                    pathlib.Path(diagnostics[0][0]).name,
                    *diagnostics[0][1:]) != expected:
                return _Failure(name, f'unexpected diagnostics: {text}')
        elif diagnostics or 'safety/exhaustive-match' in text:
            return _Failure(name, f'unexpected diagnostic: {text}')
    return None


def _documentation_check(prefix, root, environment):
    fixture = _fixture(root, 'documented', '''primitive InstalledProbe
  """Installed documentation fixture 7f13a9."""
  fun greeting(): String =>
    """Returns the installed documentation greeting."""
    "hello"
''')
    output = root / 'documentation-output'
    result = _command('documentation', [prefix / 'bin/pony-doc', '--backend',
                                       'mkdocs', '--output', output, fixture],
                      environment, root)
    failure = _successful('documentation', result)
    if failure:
        return failure
    sites = list(output.glob('*-docs'))
    if len(sites) != 1:
        return _Failure('documentation', 'expected one generated site')
    site = sites[0]
    navigation = site / 'mkdocs.yml'
    if not navigation.is_file():
        return _Failure('documentation', 'missing generated navigation')
    targets = re.findall(r':[ \t]*"?([^"\n]+\.md)"?[ \t]*$',
                         navigation.read_text(), re.MULTILINE)
    if not targets:
        return _Failure('documentation', 'empty generated navigation')
    for target in targets:
        path = (site / 'docs' / target).resolve()
        if not path.is_relative_to(site.resolve()) or not path.is_file():
            return _Failure('documentation', f'missing navigation target: '
                            f'{target}')
    pages = list((site / 'docs').glob('*InstalledProbe.md'))
    if len(pages) != 1:
        return _Failure('documentation', 'missing InstalledProbe page')
    text = pages[0].read_text()
    for expected in ('InstalledProbe', 'greeting',
                     'Installed documentation fixture 7f13a9.',
                     'Returns the installed documentation greeting.'):
        if expected not in text:
            return _Failure('documentation', f'missing content: {expected}')
    return None


def _dependency_check(prefix, root, environment):
    fixture = root / 'dependency-source'
    fixture.mkdir()
    (fixture / 'nested').mkdir()
    expected = {'main.pony': b'primitive PackedProbe\n',
                'nested/data.bin': b'\x00installed-pack\xff\n'}
    for name, data in expected.items():
        (fixture / name).write_bytes(data)
    archive = root / 'dependency.par'
    result = _command('dependency pack', [prefix / 'bin/pony-dep', 'pack',
                                          fixture, archive], environment, root)
    failure = _successful('dependency pack', result)
    if failure:
        return failure
    if not archive.is_file() or archive.stat().st_size > 4096:
        return _Failure('dependency pack', 'missing or oversized archive')
    entries = _archive_entries(archive.read_bytes())
    if isinstance(entries, _Failure):
        return entries
    files, directories = entries
    if files.keys() != expected.keys() or directories != {'nested'}:
        return _Failure('dependency pack', 'archive membership mismatch')
    for name, data in expected.items():
        if files[name] != data:
            return _Failure('dependency pack', f'content mismatch: {name}')
    return None


def _linker_check(prefix, root, environment, metadata):
    helper = prefix / 'lib/pony' / metadata['version'] / 'bin/ponylink'
    arguments = root / 'linker-arguments'
    arguments.write_text('ld.lld\n--version\n')
    result = _command('linker helper', [helper, arguments], environment, root)
    failure = _successful('linker helper', result)
    if failure:
        return failure
    if not re.search(rb'\bLLD [0-9]+\.', result.stdout + result.stderr):
        return _Failure('linker helper', 'missing LLD version response')
    missing = root / 'missing-linker-arguments'
    result = _command('linker missing file', [helper, missing], environment,
                      root)
    failure = _successful('linker missing file', result, 1)
    if failure:
        return failure
    if (b'ponylink: cannot open' not in result.stderr
            or os.fsencode(missing) not in result.stderr):
        return _Failure('linker missing file', 'missing path diagnostic')
    return None


def _agent_check(prefix, root, environment, timeout=120):
    fixture = _fixture(root, 'agent', '''primitive InstalledAgentProbe
  """Installed agent export fixture."""
''')
    peer = _Peer.start([prefix / 'bin/pony-agent-server', fixture], environment,
                       root, _LineFrames())
    if isinstance(peer, _Failure):
        return peer
    try:
        ready = peer.receive(time.monotonic() + timeout)
        if isinstance(ready, _Failure):
            return ready
        if ready.get('status') != 'ready' or ready.get('message') != (
                'compilation successful'):
            return _Failure('agent readiness', f'compilation failed: {ready}')
        failure = peer.send({'query': 'exports', 'module': str(fixture),
                             'id': 17})
        if failure:
            return failure
        response = peer.receive(time.monotonic() + timeout)
        if isinstance(response, _Failure):
            return response
        if type(response.get('id')) is not int or response['id'] != 17:
            return _Failure('agent exports', 'response ID mismatch')
        if response.get('module_path') != str(fixture):
            return _Failure('agent exports', 'module path mismatch')
        entries = response.get('entries')
        if not isinstance(entries, list) or not any(
                isinstance(entry, dict)
                and entry.get('name') == 'InstalledAgentProbe'
                and entry.get('kind') == 'primitive' for entry in entries):
            return _Failure('agent exports', 'missing primitive export')
        return peer.finish(timeout=min(timeout, 15))
    finally:
        peer.close()


def _lsp_response(peer, identifier, timeout=120):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        response = peer.receive(deadline)
        if isinstance(response, _Failure):
            return response
        if 'id' not in response:
            continue
        if type(response['id']) is not int or response['id'] != identifier:
            return _Failure('LSP response', 'response ID mismatch')
        if 'error' in response or 'result' not in response:
            return _Failure('LSP response', f'unsuccessful response: {response}')
        return response
    return _Failure('LSP response', 'deadline exceeded')


def _lsp_check(prefix, root, environment, timeout=120):
    source = '''class InstalledLspProbe
  """Installed language server fixture."""
  let _value: U32

  new create() =>
    """Sets the probe value."""
    _value = 42

  fun read(): U32 =>
    """Returns the probe value."""
    _value
'''
    fixture = _fixture(root, 'lsp', source)
    uri = (fixture / 'main.pony').as_uri()
    lines = source.splitlines()
    declaration = lines.index('  let _value: U32')
    usage = lines.index('    _value')
    expected = [{'uri': uri, 'range': {
        'start': {'line': declaration, 'character': 2},
        'end': {'line': declaration, 'character': len(lines[declaration])}}}]
    peer = _Peer.start([prefix / 'bin/pony-lsp'], environment, root,
                       _LspFrames())
    if isinstance(peer, _Failure):
        return peer
    try:
        failure = peer.send({'jsonrpc': '2.0', 'id': 1,
                             'method': 'initialize', 'params': {
                                 'processId': None,
                                 'rootUri': fixture.as_uri(),
                                 'capabilities': {'textDocument': {
                                     'publishDiagnostics': {}}}}})
        if failure:
            return failure
        response = _lsp_response(peer, 1, timeout)
        if isinstance(response, _Failure):
            return response
        for message in (
            {'jsonrpc': '2.0', 'method': 'initialized', 'params': {}},
            {'jsonrpc': '2.0', 'method': 'textDocument/didOpen', 'params': {
                'textDocument': {'uri': uri, 'languageId': 'pony',
                                 'version': 1, 'text': source}}},
        ):
            failure = peer.send(message)
            if failure:
                return failure
        deadline = time.monotonic() + timeout
        while True:
            response = peer.receive(deadline)
            if isinstance(response, _Failure):
                return response
            if 'id' in response:
                return _Failure('LSP diagnostics', 'unexpected response ID')
            if response.get('method') == 'textDocument/publishDiagnostics':
                params = response.get('params')
                if isinstance(params, dict) and params.get('uri') == uri:
                    if params.get('diagnostics') != []:
                        return _Failure('LSP diagnostics', f'{params}')
                    break
        failure = peer.send({'jsonrpc': '2.0', 'id': 2,
                             'method': 'textDocument/definition', 'params': {
                                 'textDocument': {'uri': uri},
                                 'position': {'line': usage,
                                              'character': 5}}})
        if failure:
            return failure
        response = _lsp_response(peer, 2, timeout)
        if isinstance(response, _Failure):
            return response
        if response['result'] != expected:
            return _Failure('LSP definition', f'URI/range mismatch: '
                            f'{response["result"]!r}, expected {expected!r}')
        failure = peer.send({'jsonrpc': '2.0', 'id': 3,
                             'method': 'shutdown'})
        if failure:
            return failure
        response = _lsp_response(peer, 3, timeout)
        if isinstance(response, _Failure):
            return response
        failure = peer.send({'jsonrpc': '2.0', 'method': 'exit'})
        if failure:
            return failure
        return peer.finish(timeout=min(timeout, 15))
    finally:
        peer.close()


def _run(prefix, workdir):
    metadata = _installation(prefix)
    if isinstance(metadata, _Failure):
        return metadata
    root = _scratch(workdir, prefix)
    if isinstance(root, _Failure):
        return root
    environment = _environment(root / 'home')
    operations = (
        ('compiler consumers', lambda: _compiler_checks(
            prefix, root, environment, metadata)),
        ('additive flags', lambda: _additive_checks(
            prefix, root, environment, metadata)),
        ('lint', lambda: _lint_checks(prefix, root, environment)),
        ('documentation', lambda: _documentation_check(
            prefix, root, environment)),
        ('dependency pack', lambda: _dependency_check(
            prefix, root, environment)),
        ('agent exports', lambda: _agent_check(prefix, root, environment)),
        ('language server', lambda: _lsp_check(prefix, root, environment)),
        ('linker helper', lambda: _linker_check(
            prefix, root, environment, metadata)),
    )
    for name, operation in operations:
        print(f'Checking {name}', flush=True)
        try:
            failure = operation()
        except OSError as error:
            failure = _Failure(name, str(error))
        if failure:
            return _Failure(failure.phase,
                            f'{failure.detail}; retained scratch: {root}')
    try:
        shutil.rmtree(root)
    except OSError as error:
        return _Failure('scratch cleanup', f'{root}: {error}')
    return None


def main(arguments=None):
    parser = argparse.ArgumentParser()
    parser.add_argument('--prefix', type=pathlib.Path, required=True)
    parser.add_argument('--workdir', type=pathlib.Path, required=True)
    options = parser.parse_args(arguments)
    if sys.platform != 'linux' or os.uname().machine != 'x86_64':
        print('Installed Nix checks require native x86_64 Linux', file=sys.stderr)
        return 1
    try:
        failure = _run(options.prefix.resolve(), options.workdir)
    except OSError as error:
        failure = _Failure('installed checks', str(error))
    if failure:
        print(failure, file=sys.stderr)
        return 1
    print('Installed toolchain checks passed')
    return 0


if __name__ == '__main__':
    sys.exit(main())
