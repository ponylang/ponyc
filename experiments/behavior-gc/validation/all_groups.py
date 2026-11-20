import os
import signal
import sys
import time
from pathlib import Path

session = int(sys.argv[1])
if os.getsid(session) != session:
    raise SystemExit("expected private session leader")
def members():
    pids = []
    for entry in Path('/proc').iterdir():
        if entry.name.isdecimal():
            try:
                pid = int(entry.name)
                if os.getsid(pid) == session:
                    pids.append(pid)
            except ProcessLookupError:
                pass
    return pids
while Path(f'/proc/{session}').exists() or members():
    free = next(int(line.split()[1]) for line in Path('/proc/meminfo').read_text().splitlines() if line.startswith('MemAvailable:'))
    if free < 5 * 1024 ** 2:
        while (running := members()):
            for pid in running:
                try:
                    os.kill(pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            time.sleep(0.05)
        raise SystemExit('WSL memory guard stopped the regression session')
    time.sleep(0.15)
