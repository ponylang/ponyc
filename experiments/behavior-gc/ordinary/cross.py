import io
import os
import re
import shlex
import shutil
import subprocess
import tarfile
from pathlib import Path
from run import d,repo,trace
from extra import change,prepare

image='ghcr.io/ponylang/ponyc-ci-i386-unknown-linux-alpine3.24-builder:20261109'
ssh=['ssh','-o','BatchMode=yes','-o','ConnectTimeout=5',os.getenv('PONY_ARM_SSH', 'pi@pony-rpi4-32')]
cross=d/'cross'
cross.mkdir(exist_ok=True)
def logged(args,path,**kwargs):
    with path.open('w') as f:
        subprocess.run(args,stdout=f,stderr=subprocess.STDOUT,check=True,**kwargs)
def sources(t):
    for name in ['common','libponyrt']:
        shutil.copytree(repo/'src'/name,t/'src'/name,dirs_exist_ok=True)
    shutil.copyfile(d/'clean/actor.c',t/'src/libponyrt/actor/actor.c')
    shutil.copyfile(repo/'build/build_debug/pony_generated/pony_version.h',t/'pony_version.h')
    shutil.copyfile(d/'target.sh',t/'target.sh')
    src=(repo/'src/libponyrt/CMakeLists.txt').read_text().split('set(_c_src',1)[1].split(')',1)[0]
    (t/'sources').write_text('\n'.join(src.split())+'\n')
    (t/'abi.c').write_text('''#include "actor/actor.h"
#include "mem/heap.h"
#include <stdio.h>
int main(void) {
  printf("%zu %zu %d %d %d %d\\n", (size_t)PONY_ACTOR_PAD_SIZE,
    sizeof(void*), POOL_MIN, HEAP_MIN, HEAP_MAX, POOL_ALIGN);
  return 0;
}
''')
def make_generator(target,pad):
    folder=cross/(target+'-generator')
    b=prepare(folder)
    build=repo/'build/build_debug/src'
    f=repo/'src/libponyc/codegen/codegen.c'
    data=f.read_text()
    assert data.count('PONY_ACTOR_PAD_SIZE')==2
    local=folder/'codegen.c'
    local.write_text(data.replace('PONY_ACTOR_PAD_SIZE',str(pad)))
    flags=dict(l.split(' = ',1) for l in (build/'libponyc/CMakeFiles/libponyc.dir/flags.make').read_text().splitlines() if ' = ' in l)
    args=['/usr/sbin/clang']
    for k in ['C_DEFINES','C_INCLUDES','C_FLAGS']: args+=shlex.split(flags[k])
    obj=folder/'codegen.c.o'
    subprocess.run([*args,'-I',f.parent,'-c',local,'-o',obj],check=True)
    a=folder/'libponyc.a'
    shutil.copyfile(d/'compiler/libponyc.a',a)
    subprocess.run(['ar','rcs',a,obj],check=True)
    link=shlex.split((build/'ponyc/CMakeFiles/ponyc.dir/link.txt').read_text())
    change(link,'-o',b/'ponyc')
    link[link.index('../libponyc/libponyc.a')]=str(a)
    link=[f'--dependency-file={folder}/link.d' if x.startswith('--dependency-file=') else x for x in link]
    subprocess.run(link,cwd=build/'ponyc',check=True)
    return b/'ponyc'
def generate(target,t,abi):
    pad,ptr,pool,heap,heapmax,align=map(int,abi.split())
    assert ptr==4 and (pool,heap,heapmax,align)==(32,32,512,1024)
    compiler=make_generator(target,pad)
    triple,cpu=('i686-unknown-linux-musl','pentium4') if target=='i386' else ('armv7-unknown-linux-gnueabihf','cortex-a72')
    tool=repo/'build/libs/bin'
    for case in ['graph','phase']:
        for codegen,mode in [('debug',['-d']),('optimized',[])]:
            for enabled in ['on','off']:
                env=os.environ.copy()
                env.pop('ORDINARY_SOURCE',None)
                if enabled=='on': env['ORDINARY_SOURCE']=str(d/case/'main.pony')
                label=f'work-{case}-{codegen}-{enabled}'
                part=cross/f'{target}-{label}'
                part.mkdir(exist_ok=True)
                args=[compiler,*mode,'--pic','--triple',triple,'--cpu',cpu,'-o',part,'-b','work',d/case]
                logged([*args,'-r','bitcode'],part/'compile.log',env=env)
                bits=sorted(part.glob('work.*.bc'))
                assert bits
                for bit in bits:
                    subprocess.run([tool/'llvm-dis',bit,'-o',bit.with_suffix('.ll')],check=True)
                ir=(part/'work.main.ll').read_text()
                dtype=re.search(r'^%Worker = type.*$',ir,re.M)
                if dtype: assert f'[{pad} x i8]' in dtype[0]
                descriptor=re.search(r'@Worker_Desc = [^\n]*?\{ i32 \d+, i32 \d+, i32 \d+, i32 (\d+),',ir)
                assert descriptor and int(descriptor[1])==pad+ptr
                assert dtype or re.search(r'getelementptr[^\n]+i32 '+str(pad+ptr)+r'\b',ir)
                decl=next(l for l in ir.splitlines() if l.startswith('declare') and '@pony_create(' in l)
                assert f'align {pool} dereferenceable({pad+ptr})' in decl,decl
                assert bool(re.search(r'call void @ponyint_ordinary_poll',ir))==(enabled=='on')
                merged=part/'linked.bc'
                subprocess.run([tool/'llvm-link',*bits,'-o',merged],check=True)
                lowered=part/'lowered.bc'
                assert 'LLVM version 23.1.3' in subprocess.check_output([os.getenv('LLVM_OPT', '/usr/sbin/opt'),'--version'],text=True)
                subprocess.run([os.getenv('LLVM_OPT', '/usr/sbin/opt'),'-passes=default<'+('O0' if mode else 'O2')+'>',merged,'-o',lowered],check=True)
                llc=[tool/'llc','-O='+('0' if mode else '2'),'-filetype=obj','-relocation-model=pic','-mcpu='+cpu]
                if target=='arm': llc+=['-float-abi=hard']
                subprocess.run([*llc,lowered,'-o',t/(label+'.o')],check=True)
def pack(path,files):
    with tarfile.open(path,'w:gz') as tar:
        for f in files:tar.add(f,arcname=f.name)
def target_run(target):
    t=cross/target
    t.mkdir(exist_ok=True)
    sources(t)
    if target=='arm':
        marker=t/'remote-dir'
        if not marker.exists():
            dest=subprocess.check_output([*ssh,'mktemp -d /tmp/ponyc-6490-ordinary.XXXXXXXX'],text=True).strip()
            assert re.fullmatch(r'/tmp/ponyc-6490-ordinary\.[A-Za-z0-9]{8}',dest)
            marker.write_text(dest)
        dest=marker.read_text()
        assert re.fullmatch(r'/tmp/ponyc-6490-ordinary\.[A-Za-z0-9]{8}',dest)
    def operation(stage,files):
        if target=='i386':
            cmd=['docker','run','--rm','--network=none','--memory=768m','--memory-swap=768m','--cpus=1','--user',f'{os.getuid()}:{os.getgid()}',
                 '--platform=linux/386','-v',str(t)+':/trial','-w','/trial','--entrypoint','/bin/sh',image,'target.sh',stage]
            logged(cmd,t/(stage+'.log'))
        else:
            pkg=cross/('arm-'+stage+'.tar.gz')
            pack(pkg,files)
            with pkg.open('rb') as f:
                logged([*ssh,f'cd {dest} && tar xzf - && timeout 600 sh target.sh '+stage],t/(stage+'.log'),stdin=f)
            if stage=='runtime':
                (t/'abi.txt').write_bytes(subprocess.check_output([*ssh,f'cat {dest}/abi.txt']))
            else:
                with (cross/'arm-output.tar.gz').open('wb') as f:
                    subprocess.run([*ssh,f'cd {dest} && tar czf - work-*.out work-*.elf work-*.json'],stdout=f,check=True)
                with tarfile.open(cross/'arm-output.tar.gz') as arc: arc.extractall(t,filter='data')
    operation('runtime',[t/x for x in ('src','sources','target.sh','abi.c','pony_version.h')])
    abi=(t/'abi.txt').read_text().strip()
    print(target,'pad ptr pool_min heap_min heap_max pool_align',abi,flush=True)
    generate(target,t,abi)
    operation('programs',sorted(t.glob('work-*.o')))
    for case in ['graph','phase']:
        for codegen in ['debug','optimized']:
            for enabled in ['on','off']:
                name=t/f'work-{case}-{codegen}-{enabled}'
                elf=name.with_suffix('.elf').read_text()
                assert 'ELF32' in elf and ('Intel 80386' if target=='i386' else 'ARM') in elf
                if target=='arm': assert 'hard-float ABI' in elf
                assert name.with_suffix('.out').read_bytes()==b'verified\n'
                _,gc=trace(name.with_suffix('.json'),'large')
                assert gc==((64 if case=='graph' else 2) if enabled=='on' else 0)
                print(target,case,codegen,enabled,gc,flush=True)
if __name__=='__main__':
    import sys
    for target in (sys.argv[1:] or ['i386','arm']): target_run(target)
