import csv
import hashlib
import itertools
from pathlib import Path
import subprocess
import sys

here = Path(__file__).resolve().parent
sys.path[:0] = [str(here.parent), str(here.parent / 'lexer')]
from memory import cycle_count, modules, validate_tokens
from run import compile_fixture, native_stock
from workload import fixture

DIGEST = 'a1581d0dbb9515f1aaf1c61b548cb96090fa9eb2ada4e37a4e03577705d16f59'


def timed_driver(path):
    driver = path.read_text()
    edits = [
        ('  let complete: Bool', '  let complete: Bool\n  let started: U64'),
        ('    match \\exhaustive\\ lexer(source)',
         '    started = if source.path() == "outer.pony" then Time.nanos() else 0 end\n'
         '    match \\exhaustive\\ lexer(source)'),
        ('  var _ok:', '  var _elapsed: U64 = 0\n  var _ok:'),
        ('    _one = outer', '    _elapsed = Time.nanos() - outer.started\n    _one = outer'),
        ('      inner.emit()', '      inner.emit()\n      @printf("ELAPSED %llu\\n".cstring(), _elapsed)'),
    ]
    for old, new in edits:
        if driver.count(old) != 1:
            raise ValueError(old)
        driver = driver.replace(old, new)
    path.write_text('use "time"\n' + driver)


def sample(command):
    data = subprocess.check_output(command)
    tokens, marker, duration = data.rpartition(b'ELAPSED ')
    if not marker or hashlib.sha256(tokens).hexdigest() != DIGEST:
        raise ValueError('token stream differs from experiment 3')
    elapsed = int(duration)
    if elapsed <= 0:
        raise ValueError(elapsed)
    return tokens, elapsed


def main(build, out):
    out.mkdir()
    inputs = {name: Path(name).read_bytes() for name in modules}
    src = out / 'src'
    flags = fixture(src, inputs.items(), 16)
    timed_driver(src / 'main.pony')
    stock = native_stock(out / 'stock-build')
    commands = {}
    for kind in ['stock', 'full']:
        exe = compile_fixture(build, src, out / kind, enabled=kind=='full', extra=flags, stock=stock)
        if 'Merging runtime' in (exe.parent / 'compile.log').read_text():
            raise ValueError('expected native runtime')
        for high in [False, True]:
            variant = kind + ('-high' if high else '')
            cmd = [exe, '--ponymaxthreads=1', '--ponynoscale']
            if high:
                cmd.append('--ponygcinitial=30')
            commands[variant] = cmd
            trace = out / (variant + '.json')
            tokens, _ = sample(cmd + ['--ponytracingcategories=actor_behavior,actor_gc',
                                      '--ponytracingoutput='+str(trace)])
            cycles, _ = cycle_count(trace)
            if validate_tokens(tokens, inputs, 16) != 4089 or cycles != (16 if variant=='full' else 0):
                raise ValueError((variant, cycles))
            sample(cmd)
    with (out / 'lexer-time.csv').open('w', newline='') as f:
        writer = csv.writer(f, lineterminator='\n')
        writer.writerow(['pair', 'order', 'variant', 'elapsed_ns'])
        for pair, variants in enumerate(itertools.permutations(commands)):
            for order, variant in enumerate(variants):
                _, ns = sample(commands[variant])
                writer.writerow([pair, order, variant, ns])


if __name__ == '__main__':
    main(*(Path(a).resolve() for a in sys.argv[1:]))
