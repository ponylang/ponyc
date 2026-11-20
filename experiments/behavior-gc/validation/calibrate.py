from pathlib import Path
import shutil
import subprocess

base = Path(__file__).resolve().parent
source = base / 'source-copy'
for kind, comparison in [('retained', 'original == expected'), ('replacement', 'fresh == 211')]:
    trial = base / f'calibrate-{kind}'
    trial.mkdir(exist_ok=True)
    shutil.copytree(source / 'fixture', trial / 'fixture', dirs_exist_ok=True)
    for name in ('probe.c', 'build.py', 'guard.py', 'payload-controls.sh'):
        shutil.copy(source / name, trial / name)
    probe = trial / 'probe.c'
    text = probe.read_text()
    line = f'  require({comparison}, "{kind} field payload");\n'
    if text.count(line) != 1:
        raise RuntimeError(f'expected one {line!r}')
    probe.write_text(text.replace(line, ''))
    subprocess.run(['python3', trial/'guard.py','timeout','90','python3',trial/'build.py'],check=True)
    output = base/'logs'/f'calibrate-{kind}.log'
    with output.open('w') as log:
        result=subprocess.run(['python3', trial/'guard.py','timeout','120','bash',trial/'payload-controls.sh'],
                              stderr=subprocess.STDOUT,stdout=log)
    target = f'FAIL debug off: expected rejection of {kind} field mismatch, exit 0'
    if result.returncode != 1 or target not in output.read_text().splitlines():
        raise SystemExit(f'{kind} calibration failed unexpectedly; see {output}')
    print(f'Without only {comparison!r}, the {kind} negative control correctly reported failure')
