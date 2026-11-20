from pathlib import Path
import shlex
import shutil
import subprocess

base = Path(__file__).resolve().parent
src = base / 'verified-source'
dest = base / 'bitcode'
dest.mkdir(exist_ok=True)
shutil.copytree(src/'fixture', dest/'fixture', dirs_exist_ok=True)
(dest/'bin').mkdir(exist_ok=True)
for child in (src/'bin').iterdir():
    target = dest/'bin'/child.name
    if child.is_symlink():
        if not target.exists():
            target.symlink_to(child.resolve())
    else:
        shutil.copy2(child, target)
for name in ('probe.c','trace.py'):
    shutil.copy2(src/name,dest/name)
repo = Path.cwd()
if not (dest/'packages').exists():
    (dest/'packages').symlink_to(repo/'packages')
rt = repo/'build/build_debug/src/libponyrt'
makefile = (rt/'CMakeFiles/libponyrt_bc.dir/build.make').read_text().splitlines()
def recipe(needle):
    line, = (line for line in makefile if line.startswith('\tcd ') and needle in line)
    return shlex.split(line.strip().split(' && ')[-1])
def replace_flag(cmd, flag, value):
    cmd[cmd.index(flag) + 1] = str(value)
source = (src/'fixture/main.pony').read_text().splitlines()
site, = [(i,line.index('=')+1) for i,line in enumerate(source,1) if 'let replacement =' in line]
objects = {}
for member, override in [('actor/actor.c', src/'objects/actor.c'),('gc/root_audit.c',src/'probe.c')]:
    args = recipe(' -c ' + str(repo/'src/libponyrt'/member))
    out = dest / (Path(member).name + '.bc')
    objects[str(rt/(member+'.bc'))] = str(out)
    replace_flag(args,'-c',override)
    replace_flag(args,'-o',out)
    replace_flag(args,'-MF',str(out)+'.d')
    args += ['-I',str(repo/'src/libponyrt/actor'),f'-DSITE_LINE={site[0]}',f'-DSITE_COLUMN={site[1]}']
    subprocess.run(args,check=True)
args=recipe('/llvm-link -o ')
replace_flag(args,'-o',dest/'bin/libponyrt.bc')
for target in objects:
    if args.count(target)!=1: raise ValueError(target)
args=[objects.get(arg,arg) for arg in args]
subprocess.run(args,check=True)
