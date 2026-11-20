import json
import os
import subprocess
from run import configure,d,out,run,trace
src=d/'recovery/main.pony'
for variant in ['observed','clean','stock']:
    folder=out/('recovery-'+variant)
    folder.mkdir(exist_ok=True)
    configure(variant)
    env=os.environ.copy()
    env.pop('ORDINARY_SOURCE',None)
    if variant!='stock':env['ORDINARY_SOURCE']=str(src)
    run([d/'bin/ponyc','--pic','-o',folder,'-b','work',src.parent],env,folder/'compile.log')
    for threads in [1,2]:
        j=folder/f'{threads}.json'
        j.unlink(missing_ok=True)
        p=subprocess.run([folder/'work',f'--ponymaxthreads={threads}','--ponynoscale','--ponytracingcategories=actor_behavior,actor_gc','--ponytracingoutput='+str(j)],capture_output=True,check=True)
        assert p.stdout.splitlines()==[b'finalized',b'verified']
        addr,gcs=trace(j,'small')
        assert gcs==int(variant!='stock')
        events=json.loads(j.read_text())
        bounds=[(i,e['args']['behavior_name'],e['ph']) for i,e in enumerate(events)
                if e['name']=='actor_behavior_run' and e['id']==addr and e['args'].get('behavior_name') in ['Worker.first','Worker.run_small','Worker.last']]
        assert [(name,ph) for _,name,ph in bounds]==[(n,p) for n in ['Worker.first','Worker.run_small','Worker.last'] for p in ['b','e']]
        assert not any(e['name']=='actor_gc' and e['id']==addr for e in events[bounds[0][0]+1:bounds[1][0]])
        j.with_suffix('.stderr').write_bytes(p.stderr)
        if variant=='observed':
            rows=[r.split() for r in p.stderr.decode().splitlines()]
            assert len(rows)==3 and all(r[:2]==['poll',addr] for r in rows)
            assert all(int(r[3])>int(r[4]) for r in rows)
            assert [r[5:7] for r in rows]==[['0','0'],['0','0'],['1','1']]
        print(variant,threads,'FIFO first -> run_small -> last; nested collections',gcs)
