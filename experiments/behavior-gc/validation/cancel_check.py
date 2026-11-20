from pathlib import Path
import os
import signal
import subprocess
import sys
import tempfile
import time

guard = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as tmp:
    for signum in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
        ready = Path(tmp) / str(signum)
        program = """import os,sys,subprocess,time
from pathlib import Path
child = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(15)'], process_group=0)
Path(sys.argv[1]).write_text(f'{os.getpid()} {child.pid}')
time.sleep(15)
"""
        handler = subprocess.Popen([sys.executable, guard, sys.executable, '-c', program, ready])
        try:
            for attempt in range(150):
                if ready.exists() and len(ready.read_text().split()) == 2:
                    break
                time.sleep(0.02)
            else:
                raise RuntimeError('child readiness timed out')
            handler.send_signal(signum)
            status = handler.wait(timeout=5)
            if status != 128 + signum:
                raise RuntimeError(f'{signum.name}: guard exited {status}')
            for pid in ready.read_text().split():
                stat = Path(f'/proc/{pid}/stat')
                if stat.exists() and stat.read_text().rsplit(')', 1)[1].split()[0] != 'Z':
                    raise RuntimeError(f'{signum.name} left child {pid} running')
            print(f'{signum.name}: guard and both private process groups stopped')
        finally:
            if handler.poll() is None:
                handler.kill()
                handler.wait()
            if ready.exists():
                for pid in ready.read_text().split():
                    try:
                        os.kill(int(pid), signal.SIGKILL)
                    except ProcessLookupError:
                        pass
