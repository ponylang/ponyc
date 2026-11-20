import os
import subprocess
from run import d,out,configure,trace,run

src=d/'refusal/main.pony'
folder=out/'refusal'
folder.mkdir(exist_ok=True)
configure('observed')
env=os.environ.copy()
env['ORDINARY_SOURCE']=str(src)
run([d/'bin/ponyc','--pic','-b','work','-o',folder,src.parent],env,folder/'compile.log')
for mode in ['ordinary','final','foreign','local']:
    j=folder/(mode+'.json')
    j.unlink(missing_ok=True)
    p=subprocess.run([folder/'work',mode,'--ponymaxthreads=1','--ponytracingcategories=actor_behavior,actor_gc','--ponytracingoutput='+str(j)],capture_output=True,check=True)
    j.with_suffix('.out').write_bytes(p.stdout)
    j.with_suffix('.err').write_bytes(p.stderr)
    addr,n=trace(j,'small')
    words=p.stderr.decode().split()
    assert len(words)==11 and words[:2]==['poll',addr]
    site,before,threshold,eligible,done,after,cap,post,reuse=map(int,words[2:])
    assert site==src.read_text().splitlines().index('    verify()')+1 and before>threshold
    assert eligible==done==n==int(mode=='ordinary'),(mode,words,n)
    assert p.stdout.decode().splitlines()=={'ordinary':['verified'],'foreign':['verified'],'final':['verified','finalized'],'local':['verified','queued']}[mode]
    print(mode,before,threshold,eligible,n,flush=True)
