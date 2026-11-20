from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

guard = Path(sys.argv[1]).resolve()
mib = 1024 * 1024
start = 11 * mib // 2
floor = 5 * mib
bootstrap = """import runpy, sys
from pathlib import Path
guard, marker, initial, running = sys.argv[1:]
read = Path.read_text
def read_fixture(self, *args, **kwargs):
    if self == Path('/proc/meminfo'):
        level = running if Path(marker).exists() else initial
        return f'MemAvailable: {level} kB\\n'
    return read(self, *args, **kwargs)
Path.read_text = read_fixture
child = "from pathlib import Path; import sys,time; Path(sys.argv[1]).touch(); time.sleep(0.8)"
sys.argv = [guard, sys.executable, '-c', child, marker]
runpy.run_path(guard, run_name='__main__')
"""
cases = (
    (start - 1, floor, False, 'Insufficient WSL headroom to start'),
    (start, floor, True, ''),
    (start, floor - 1, True, 'Stopping experiment below 5 GiB WSL headroom'),
)
with tempfile.TemporaryDirectory() as temp:
    copy = Path(temp) / 'guard.py'
    shutil.copy(guard, copy)
    for i, (initial, running, launched, error) in enumerate(cases):
        marker = Path(temp) / f'started-{i}'
        result = subprocess.run([sys.executable, '-c', bootstrap, str(copy), str(marker),
                                 str(initial), str(running)], capture_output=True, text=True, timeout=5)
        if (marker.exists(), result.returncode, result.stderr.strip()) != (launched, int(bool(error)), error):
            raise SystemExit(f'FAIL initial={initial} running={running} launched={marker.exists()} '
                             f'exit={result.returncode} diagnostic={result.stderr.strip()!r}')
        print(f'PASS initial={initial} KiB running={running} KiB started={launched} stopped={bool(error)}')
