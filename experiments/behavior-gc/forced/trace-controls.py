import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

trial = Path(__file__).resolve().parent


def check(path, setting, diagnostic=None):
    for optimize in ('0', '1'):
        result = subprocess.run([sys.executable, trial / 'trace.py', path, setting],
                                env=dict(os.environ, PYTHONOPTIMIZE=optimize),
                                capture_output=True, text=True)
        if diagnostic is None:
            if result.returncode != 0:
                raise SystemExit(result.stderr)
        elif result.returncode != 1 or f'invalid trace: {diagnostic}' not in result.stderr:
            raise SystemExit(f'{path.name} optimize={optimize} expected {diagnostic!r}, '
                             f'got {result.returncode} {result.stderr!r}')


for mode in ('debug', 'optimized'):
    for setting, opposite in (('on', 'off'), ('off', 'on')):
        path = trial / 'logs' / f'{mode}-{setting}.json'
        check(path, setting)
        check(path, opposite, 'expected collection phases')

original = json.loads((trial / 'logs/optimized-on.json').read_text())
bounds = [i for i, event in enumerate(original)
          if event['name'] == 'actor_behavior_run'
          and event['args'].get('behavior_name') == 'Probe.measure']
start, end = bounds
begin_gc, end_gc = [i for i in range(start + 1, end)
                    if original[i]['name'] == 'actor_gc'
                    and original[i]['id'] == original[start]['id']]
t0, t1 = original[start]['ts'], original[end]['ts']
cases = (
    (end, 'ph', 'b', 'expected one complete'),
    (end, 'id', 'different-actor', 'behavior endpoints differ'),
    (end, 'tid', -1, 'behavior endpoints differ'),
    (end, 'ts', t0 - 1, 'behavior timestamps are reversed'),
    (end_gc, 'ph', 'b', 'expected collection phases'),
    (end_gc, 'id', 'different-actor', 'expected collection phases'),
    (begin_gc, 'ts', t0 - 1, 'collection timestamps'),
    (end_gc, 'ts', t1 + 1, 'collection timestamps'),
    (end_gc, 'ts', original[begin_gc]['ts'] - 1, 'collection timestamps'),
    (begin_gc, 'tid', -1, 'collection thread differs'),
    (end_gc, 'tid', -1, 'collection thread differs'),
)
with tempfile.TemporaryDirectory() as temporary:
    path = Path(temporary) / 'edited-trace.json'
    for index, field, value, diagnostic in cases:
        events = original.copy()
        events[index] = {**events[index], field: value}
        path.write_text(json.dumps(events))
        check(path, 'on', diagnostic)
    off = json.loads((trial / 'logs/optimized-off.json').read_text())
    for category in ('actor_gc', 'actor_behavior'):
        header = dict(off[0]['args'])
        header['categories_enabled'] = [c for c in header['categories_enabled'] if c != category]
        path.write_text(json.dumps([{**off[0], 'args': header}, *off[1:]]))
        check(path, 'off', 'trace must record')
    for partial in (off[1:], off[:-1]):
        path.write_text(json.dumps(partial))
        check(path, 'off', 'expected a complete trace')
print('Matching, opposite, malformed and incomplete recordings checked in both Python modes')
