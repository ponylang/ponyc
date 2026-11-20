import os
import signal
import subprocess
import sys
import time
from pathlib import Path


def available():
    for line in Path('/proc/meminfo').read_text().splitlines():
        if line.startswith('MemAvailable:'):
            return int(line.split()[1]) / (1024 * 1024)
    raise RuntimeError('MemAvailable missing')


def stop_session(session):
    while True:
        found = False
        for entry in Path('/proc').iterdir():
            if not entry.name.isdecimal():
                continue
            pid = int(entry.name)
            try:
                if os.getsid(pid) != session:
                    continue
                if (entry / 'stat').read_text().rsplit(')', 1)[1].split()[0] == 'Z':
                    continue
                os.kill(pid, signal.SIGKILL)
                found = True
            except (FileNotFoundError, ProcessLookupError):
                pass
        if not found:
            return
        time.sleep(0.05)


cancelled = None

def request_stop(signum, frame):
    global cancelled
    cancelled = signum

for signum in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
    signal.signal(signum, request_stop)

if available() < 5.5:
    sys.exit('Insufficient WSL headroom to start')
logs = Path(__file__).resolve().parent / 'logs'
logs.mkdir(exist_ok=True)
with (logs / 'memory.tsv').open('a', buffering=1) as log:
    if cancelled is not None:
        sys.exit(128 + cancelled)
    job = subprocess.Popen(sys.argv[1:], start_new_session=True)
    try:
        while True:
            if cancelled is not None:
                sys.exit(128 + cancelled)
            remaining = available()
            print(f'{time.time():.2f}\t{remaining:.3f}', file=log)
            if remaining < 5:
                sys.exit('Stopping experiment below 5 GiB WSL headroom')
            status = job.poll()
            if status is not None:
                sys.exit(status if status >= 0 else 128 - status)
            time.sleep(0.25)
    finally:
        stop_session(job.pid)
        job.wait()
