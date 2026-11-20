import os
import shlex
import shutil
import subprocess
from pathlib import Path
from run import d,repo,out,trace,run

def change(args,flag,value):
    args[args.index(flag)+1]=str(value)
def prepare(dir):
    b=dir/'bin'
    b.mkdir(parents=True,exist_ok=True)
    for f in (repo/'build/debug').glob('crt*.o'):
        if not (b/f.name).exists(): (b/f.name).symlink_to(f)
    if not (dir/'packages').exists(): (dir/'packages').symlink_to(repo/'packages')
    return b
def bitcode():
    dir=d/'merged'
    b=prepare(dir)
    if not (b/'ponyc').exists(): os.link(d/'bin/ponyc',b/'ponyc')
    if not (b/'libponyrt-pic.a').exists(): (b/'libponyrt-pic.a').symlink_to(d/'clean/libponyrt-pic.a')
    r=repo/'build/build_debug/src/libponyrt'
    lines=(r/'CMakeFiles/libponyrt_bc.dir/build.make').read_text().splitlines()
    def recipe(needle):
        cmd,=(l for l in lines if l.startswith('\tcd ') and needle in l)
        return shlex.split(cmd.split(' && ')[-1])
    args=recipe(' -c '+str(repo/'src/libponyrt/actor/actor.c'))
    change(args,'-c',d/'clean/actor.c')
    change(args,'-o',dir/'actor.bc')
    change(args,'-MF',dir/'actor.d')
    subprocess.run([*args,'-I',repo/'src/libponyrt/actor'],check=True)
    args=recipe('/llvm-link -o ')
    args[args.index(str(r/'actor/actor.c.bc'))]=str(dir/'actor.bc')
    change(args,'-o',b/'libponyrt.bc')
    subprocess.run(args,check=True)
    return b
def sanitized():
    dir=d/'sanitized'
    b=prepare(dir)
    old=Path(os.getenv('ORDINARY_ASAN_BUILD',d.parent/'asan'))
    tree=old/'config/src'
    stock,=old.glob('debug-*')
    for lib,name,sub,source,archive in [
        ('libponyc','gencontrol.c','codegen',d/'compiler/gencontrol.c',tree/'libponyc/libponyc.a'),
        ('libponyrt','actor.c','actor',d/'clean/actor.c',stock/'libponyrt-pic.a')]:
        flags=dict(line.split(' = ',1) for line in (tree/lib/'CMakeFiles'/f'{lib}.dir/flags.make').read_text().splitlines() if ' = ' in line)
        cmd=['/usr/sbin/clang']
        for k in ['C_DEFINES','C_INCLUDES','C_FLAGS']:cmd.extend(shlex.split(flags[k]))
        obj=dir/(name+'.o')
        subprocess.run([*cmd,'-I',repo/'src'/lib/sub,'-c',source,'-o',obj],check=True)
        dst=(dir if lib=='libponyc' else b)/archive.name
        shutil.copy2(archive,dst)
        subprocess.run(['ar','rcs',dst,obj],check=True)
    args=shlex.split((tree/'ponyc/CMakeFiles/ponyc.dir/link.txt').read_text())
    args[args.index('../libponyc/libponyc.a')]=str(dir/'libponyc.a')
    change(args,'-o',b/'ponyc')
    args=[f'--dependency-file={dir}/link.d' if x.startswith('--dependency-file=') else x for x in args]
    subprocess.run(args,cwd=tree/'ponyc',check=True)
    return b
def test(b,case,mode,flags):
    path=out/f'extra-{case}-{mode}'
    path.mkdir(exist_ok=True)
    env=os.environ.copy()
    env['ORDINARY_SOURCE']=str(d/case/'main.pony')
    env['ASAN_OPTIONS']='detect_leaks=0:halt_on_error=1'
    env['UBSAN_OPTIONS']='halt_on_error=1'
    run([b/'ponyc',*flags,'--pic','-b','work','-o',path,d/case],env,path/'compile.log')
    if 'merged' in mode: assert 'Merging runtime' in (path/'compile.log').read_text().splitlines()
    for threads in [1,2]:
        for noblock in [False,True]:
            args=[path/'work','large',f'--ponymaxthreads={threads}','--ponynoscale']
            if noblock: args+=['--ponynoblock']
            j=path/f'{threads}-{noblock}.json'
            j.unlink(missing_ok=True)
            p=subprocess.run([*args,'--ponytracingcategories=actor_behavior,actor_gc','--ponytracingoutput='+str(j)],env=env,capture_output=True)
            j.with_suffix('.stderr').write_bytes(p.stderr)
            assert p.returncode==0 and p.stdout==b'verified\n' and not p.stderr,p.stderr[:1000]
            assert trace(j,'large')[1]==(2 if case=='phase' else 64)
            print(case,mode,threads,noblock,'passed',flush=True)
if __name__=='__main__':
    b=bitcode()
    for mode,flags in [('debug',['-d']),('optimized',[])]:
        for lto in ['fat','thin']:
            for case in ['graph','phase']:
                test(b,case,'merged-'+mode+'-'+lto,[*flags,'--'+lto+'-lto'])
    b=sanitized()
    for mode,flags in [('debug',['-d']),('optimized',[])]:
        for case in ['graph','phase']: test(b,case,'sanitized-'+mode,flags)
