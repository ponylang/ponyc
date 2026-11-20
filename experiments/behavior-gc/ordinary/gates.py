import os
import shutil
import shlex
import subprocess
from pathlib import Path
from run import d,repo,out,run,trace
from extra import prepare
folder=out/'gates'
b=prepare(folder)
if not (b/'ponyc').exists(): os.link(d/'bin/ponyc',b/'ponyc')
src=d/'observed/actor.c'
s=src.read_text()
key='  if(eligible)\n'
assert s.count(key)==1
s=s.replace(key,'''  fprintf(stderr, "guards %d %d %zu %zu %zu %d\\n", ctx->stack != NULL,
    ctx->msg_target != NULL, ponyint_actormap_size(&ctx->acquire),
    ponyint_objectmap_size(&actor->gc.local), ponyint_actormap_size(&actor->gc.foreign),
    ponyint_heap_has_finalisers(&actor->heap));
'''+key)
path=folder/'actor.c'
path.write_text(s)
f=repo/'build/build_debug/src/libponyrt/CMakeFiles/libponyrt.dir/flags.make'
flags=dict(l.split(' = ',1) for l in f.read_text().splitlines() if ' = ' in l)
args=['/usr/sbin/clang']
for k in ['C_DEFINES','C_INCLUDES','C_FLAGS']:args+=shlex.split(flags[k])
obj=folder/'actor.c.o'
subprocess.run([*args,'-DORDINARY_OBSERVE','-I',repo/'src/libponyrt/actor','-c',path,'-o',obj],check=True)
archive=b/'libponyrt-pic.a'
shutil.copyfile(d/'observed/libponyrt-pic.a',archive)
subprocess.run(['ar','rcs',archive,obj],check=True)
src=d/'refusal/main.pony'
env=os.environ.copy()
env['ORDINARY_SOURCE']=str(src)
run([b/'ponyc','--pic','-o',folder,'-b','work',src.parent],env,folder/'compile.log')
for mode,mask in [('ordinary',[]),('local',[3]),('foreign',[4]),('final',[5])]:
    j=folder/(mode+'.json')
    j.unlink(missing_ok=True)
    p=subprocess.run([folder/'work',mode,'--ponymaxthreads=1','--ponytracingcategories=actor_behavior,actor_gc','--ponytracingoutput='+str(j)],capture_output=True,check=True)
    j.with_suffix('.stderr').write_bytes(p.stderr)
    assert p.stdout.decode().splitlines()=={'ordinary':['verified'],'foreign':['verified'],'final':['verified','finalized'],'local':['verified','queued']}[mode]
    guard,poll=p.stderr.decode().splitlines()
    bits=guard.split()
    assert bits[0]=='guards' and len(bits)==7
    assert [i for i,strval in enumerate(bits[1:]) if int(strval)>0]==mask,(mode,bits,mask)
    addr,gcs=trace(j,'small')
    record=poll.split()
    assert record[:2]==['poll',addr] and int(record[3])>int(record[4])
    assert int(record[5])==int(record[6])==gcs==int(mode=='ordinary')
    print(mode,guard,gcs,flush=True)
