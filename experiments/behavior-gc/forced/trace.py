import argparse
import json
from pathlib import Path


def check_trace(events, enabled):
    if not events or events[0]['name'] != 'start_trace' or events[-1]['name'] != 'end_trace':
        raise ValueError('expected a complete trace recording')
    categories = events[0]['args']['categories_enabled']
    if not {'actor_behavior', 'actor_gc'}.issubset(categories):
        raise ValueError('trace must record actor_behavior and actor_gc')
    bounds = [(i, event) for i, event in enumerate(events)
              if event['name'] == 'actor_behavior_run'
              and event['args'].get('behavior_name') == 'Probe.measure']
    if [event['ph'] for _, event in bounds] != ['b', 'e']:
        raise ValueError('expected one complete Probe.measure behavior')
    (first, start), (last, end) = bounds
    if (start['id'], start['tid']) != (end['id'], end['tid']):
        raise ValueError('behavior endpoints differ in actor or thread')
    if end['ts'] < start['ts']:
        raise ValueError('behavior timestamps are reversed')
    nested = [event for event in events[first + 1:last]
              if event['name'] == 'actor_gc' and event['id'] == start['id']]
    expected = ['b', 'e'] if enabled else []
    if [event['ph'] for event in nested] != expected:
        raise ValueError(f'expected collection phases {expected}')
    times = [start['ts'], *(event['ts'] for event in nested), end['ts']]
    if times != sorted(times):
        raise ValueError('collection timestamps are outside the behavior or reversed')
    if any(event['tid'] != start['tid'] for event in nested):
        raise ValueError('collection thread differs from behavior thread')
    return len(nested) // 2


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('trace', type=Path)
    parser.add_argument('mode', choices=('off', 'on'))
    args = parser.parse_args()
    try:
        count = check_trace(json.loads(args.trace.read_text()), args.mode == 'on')
    except (ValueError, KeyError, TypeError) as error:
        parser.exit(1, f'invalid trace: {error}\n')
    print(f'Probe.measure enclosed {count} complete GC cycles')
