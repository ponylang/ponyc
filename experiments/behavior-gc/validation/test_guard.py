import os
import runpy
import subprocess
import sys
import time
from pathlib import Path

root = Path(__file__).resolve().parent
ready = root / 'guard-test-children'
child_source = """import os,sys,time,subprocess
from pathlib import Path
child = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(18)'], process_group=0)
Path(sys.argv[1]).write_text(str(os.getpid()) + ' ' + str(child.pid))
time.sleep(18)
"""
guard = root / 'source-copy' / 'guard.py'
read_text = Path.read_text
def test_read_text(self, *args, **kwargs):
    if self == Path('/proc/meminfo'):
        return 'MemAvailable: ' + ('4000000' if ready.exists() else '16000000') + ' kB\n'
    return read_text(self, *args, **kwargs)
ready.unlink(missing_ok=True)
sys.argv = [str(guard), sys.executable, '-c', child_source, str(ready)]
Path.read_text = test_read_text
try:
    runpy.run_path(str(guard), run_name='__main__')
except SystemExit as exc:
    if str(exc) != 'Stopping experiment below 5 GiB WSL headroom':
        raise
else:
    raise RuntimeError('simulated memory pressure did not stop the job')
finally:
    Path.read_text = read_text
for pid in ready.read_text().split():
    stat = Path(f'/proc/{pid}/stat')
    if stat.exists() and stat.read_text().rsplit(')',1)[1].split()[0] != 'Z':
        raise RuntimeError('descendant still running: ' + pid)
print('Simulated 5 GiB floor stopped both process groups')
