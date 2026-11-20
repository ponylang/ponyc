import csv
import os
from pathlib import Path
import sys
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'threshold'))
from benchmark import build, check_calls, compile_fixture, lexer, micro, native_stock, write_csv
from inspect_maps import inspect

KINDS = ('stock', 'eager', 'maps', 'check', 'mapped')


def compile_one(builds, kind, src, out, *, extra=(), debug=False):
    with patch.dict(os.environ):
        os.environ.pop('GC_STACKMAP_CONTROL', None)
        if kind in ('maps', 'check'):
            os.environ['GC_STACKMAP_CONTROL'] = kind
        exe = compile_fixture(builds['eager'] if kind == 'eager' else builds['mapped'],
                              src, out, debug, kind != 'stock', extra, builds['stock'])
    if 'Merging runtime' in (out/'compile.log').read_text():
        raise ValueError('expected native runtime archive')
    if kind in ('maps', 'mapped', 'check'):
        rows = inspect(exe, kind)
        if rows:
            write_csv(out/'sites.csv', rows)
    return exe


def require_sites(exe, kind, poll_count):
    if kind in ('maps', 'mapped'):
        roles = [row['role'] for row in inspect(exe, kind)]
        if roles.count(2) != 1 or roles.count(3) != 1 or roles.count(4) != (poll_count if kind == 'mapped' else 0):
            raise ValueError(('incomplete frame maps', kind, roles))


def micro_checks(exe, kind):
    require_sites(exe, kind, 1)
    if kind in ('stock', 'eager'):
        check_calls(exe, 'full' if kind == 'eager' else 'stock')
    else:
        from run import native_calls
        phase = native_calls(exe, 'Probe_ref_phase_o')
        assert ('ponyint_threshold_due' in phase) == (kind != 'maps')
        assert ('ponyint_mapped_collect' in phase) == (kind == 'mapped')
        assert 'Probe_ref_middle_o' in native_calls(exe, 'Probe_tag_measure_o')
        assert 'Probe_ref_phase_o' in native_calls(exe, 'Probe_ref_middle_o')


def measure(builds, out):
    for series in (1, 2):
        micro(builds, out/f'micro-{series}', kinds=KINDS, compile_one=compile_one,
              inspect=micro_checks, seed=6490+series)
    lexer(builds, out/'lexer', kinds=KINDS, compile_one=compile_one,
          inactive=('maps', 'stock', 'check'))
    for kind in ('mapped', 'maps'):
        require_sites(out/'lexer'/kind/'work', kind, 16)
    with (out/'lexer/lexer-memory.csv').open() as f:
        for row in csv.DictReader(f):
            expected = 16 if row['threshold']=='default' and row['variant'] in ('mapped','eager') else 0
            if int(row['cycles']) != expected:
                raise ValueError(row)


def main(out):
    builds = {'stock': native_stock(out/'stock-build'), 'eager': out/'eager-build',
              'mapped': out/'map-build'}
    build(builds['eager'])
    build(builds['mapped'], stackmap=True)
    from verify import safety, lexer_slots
    safety(builds, out/'safety')
    measure(builds, out)
    lexer_slots(out/'lexer/mapped/work', builds['mapped']/'actor.c')


if __name__ == '__main__':
    main(Path(sys.argv[1]).resolve())
