import csv
import os
import json
import subprocess
from run import d,out,run,configure
src=d/'timed/main.pony'
src.parent.mkdir(exist_ok=True)
code=(d/'cost/main.pony').read_text()
code='use "time"\n'+code.replace('  be tick() =>\n','  be tick() =>\n    begin_interval()\n')
code=code.replace('  var _round:', '  var _start: U64 = 0\n  var _elapsed: U64 = 0\n  var _round:')
code=code.replace('  fun ref step()', '  fun ref begin_interval() => _start = Time.nanos()\n\n  fun ref step()')
code=code.replace('    _round = _round + 1','    _elapsed = _elapsed + (Time.nanos() - _start)\n    _round = _round + 1')
code=code.replace('"%llu\\n".cstring(), _x','"%llu %llu\\n".cstring(), _x, _elapsed')
src.write_text(code)
value=7
for _ in range(16384*64): value=((value*6364136223846793005)+1442695040888963407) % (1<<64)
for v in ['clean','stock']:
    folder=out/('timed-'+v)
    folder.mkdir(exist_ok=True)
    configure(v)
    env=os.environ.copy()
    env.pop('ORDINARY_SOURCE',None)
    if v=='clean': env['ORDINARY_SOURCE']=str(src)
    args=[d/'bin/ponyc','--pic','-o',folder,'-b','work',src.parent]
    run([*args,'-r','ir'],env,folder/'ir.log')
    calls=(folder/'work.main.ll').read_text().count('call void @ponyint_ordinary_poll(')
    assert calls==(65 if v=='clean' else 0)
    run(args,env,folder/'compile.log')
    j=folder/'trace.json'
    j.unlink(missing_ok=True)
    p=subprocess.run([folder/'work','--ponymaxthreads=1','--ponytracingcategories=actor_behavior,actor_gc','--ponytracingoutput='+str(j)],capture_output=True,check=True)
    events=json.loads(j.read_text())
    assert events[0]['name']=='start_trace' and events[-1]['name']=='end_trace'
    assert {'actor_gc','actor_behavior'}<=set(events[0]['args']['categories_enabled'])
    inside=None
    count=0
    for e in events:
        if e['name']=='actor_behavior_run' and e['args'].get('behavior_name')=='Worker.tick':
            if e['ph']=='b':
                assert inside is None
                inside=e['id']
            else:
                assert e['ph']=='e' and inside==e['id']
                count+=1
                inside=None
        if e['name']=='actor_gc': assert e['id']!=inside
    assert inside is None and count==16384
    assert int(p.stdout.split()[0])==value and not p.stderr
with (d/'internal-cost.csv').open('w') as f:
    for rep in range(20):
        for v in (['clean','stock'] if rep%2==0 else ['stock','clean']):
            folder=out/('timed-'+v)
            p=subprocess.run([folder/'work','--ponymaxthreads=1'],capture_output=True,check=True)
            checksum,ns=map(int,p.stdout.split())
            assert checksum==value and ns>0 and not p.stderr
            csv.writer(f).writerow([rep,v,ns])
print('20 alternating pairs, 16384 handler intervals each')
