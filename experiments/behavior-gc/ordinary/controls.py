import json
import os
import re
import subprocess
from pathlib import Path
from run import d, out, configure, run, trace, records
src=d/'exclusions/main.pony'
configure('clean')
for mode,flags in [('normal',[]),('wrong-file',[]),('audit',['--gc-root-audit']),('mutate',['--mutate'])]:
    directory=out/('controls-'+mode)
    directory.mkdir(exist_ok=True)
    env=os.environ.copy()
    env['ORDINARY_SOURCE']=str(src if mode!='wrong-file' else d/'graph/main.pony')
    run([d/'bin/ponyc','-d','--pic','-r','ir','-b','control','-o',directory,*flags,src.parent],env,directory/'compile.log')
    ir=(directory/'control.main.ll').read_text()
    sites=re.findall(r'call void @ponyint_ordinary_poll\([^\n]*?i32(?: [a-z]+)* (\d+)\)',ir)
    expected=[str(src.read_text().splitlines().index('  be eligible() =>')+3)] if mode=='normal' else []
    assert sites==expected,(mode,sites,expected)
    print('IR',mode,sites)
def reject(label, function, *args):
    try: function(*args)
    except (AssertionError, KeyError, ValueError):
        print('rejected',label)
    else: raise RuntimeError('accepted invalid '+label)
folder=out/'graph-optimized-observed'
file=folder/'small-observed.json'
addr,count=trace(file,'small')
raw=folder/'small-observed.err'
sites=[int(s.split()[2]) for s in raw.read_text().splitlines()]
control=out/'control-events.json'
events=json.loads(file.read_text())
control.write_text(json.dumps(events[:-1]))
reject('incomplete trace',trace,control,'small')
control.write_text(json.dumps([e for e in events if not(e['name']=='actor_gc' and e['id']==addr and e['ph']=='e')]))
reject('unpaired collection',trace,control,'small')
for col,val,label in [(1,'0x0','actor'),(2,'0','site'),(6,'0','completion'),(8,'0','capacity')]:
    lines=[s.split() for s in raw.read_text().splitlines()]
    lines[1][col]=val
    tmp=out/'control-poll.txt'
    tmp.write_text('\n'.join(' '.join(x) for x in lines))
    reject(label,records,tmp,addr,sites,True,count)
reject('collection disabled',records,folder/'small-sample.err',trace(folder/'small-sample.json','small')[0],sites,True,count)
for case in ['graph','regex','grammar']:
    directory=out/f'{case}-optimized-clean'
    j=directory/'large-raised-threshold.json'
    j.unlink(missing_ok=True)
    p=subprocess.run([directory/'work','large','--ponymaxthreads=1','--ponygcinitial=30',
                      '--ponytracingcategories=actor_behavior,actor_gc','--ponytracingoutput='+str(j)],capture_output=True,check=True)
    assert p.stdout==b'verified\n' and trace(j,'large')[1]==0
    print(case,'raised threshold enclosed zero collections')
