from pathlib import Path
import shutil
base=Path(__file__).resolve().parent
trial=base/'asan-fixture'
src=base/'verified-source'
build=base/'asan/config'
out, = (base/'asan').glob('debug-*')
for name in ('ponyc','libponyrt-pic.a','crtbeginS.o','crtbeginT.o','crtend.o','crtendS.o'):
    if not (out/name).is_file():
        raise FileNotFoundError(out/name)
trial.mkdir(exist_ok=True)
shutil.copytree(src/'fixture',trial/'fixture',dirs_exist_ok=True)
for name in ['guard.py','trace.py','trace-controls.py','payload-controls.sh','run.sh','probe.c']:
    shutil.copy2(src/name,trial/name)
original=(src/'build.py').read_text()
for part,path in [
    ("repo / 'build/build_debug/src/libponyrt/CMakeFiles/libponyrt.dir/flags.make'", build/'src/libponyrt/CMakeFiles/libponyrt.dir/flags.make'),
    ("repo / 'build/debug/libponyrt-pic.a'", out/'libponyrt-pic.a'),
    ("repo / 'build/debug/ponyc'",out/'ponyc'),
    ("repo / 'build/debug'",out)]:
    if original.count(part)!=1: raise ValueError(part)
    original=original.replace(part, f'Path({str(path)!r})')
(trial/'build.py').write_text(original)
