import csv
import json
import os
import subprocess
from pathlib import Path
from run import configure,d,out,run

def main():
    src=d/'cost/main.pony'
    src.parent.mkdir(exist_ok=True)
    src.write_text('''use @printf[I32](fmt: Pointer[U8] tag, ...)
use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    @pony_exitcode(1)
    Worker

actor Worker
  var _round: USize = 0
  var _x: U64 = 7
  new create() => tick()

  be tick() =>
'''+('    step()\n'*64)+'''    finish()

  fun ref step() =>
    _x = (_x * 6364136223846793005) + 1442695040888963407

  fun ref finish() =>
    _round = _round + 1
    if _round < 16384 then
      tick()
    else
      @printf("%llu\\n".cstring(), _x)
      @pony_exitcode(0)
    end
''')
    value=7
    for _ in range(16384*64):
        value=(value*6364136223846793005+1442695040888963407) & ((1<<64)-1)
    expected=f'{value}\n'.encode()
    for variant in ['clean','stock']:
        folder=out/('cost-'+variant)
        folder.mkdir(exist_ok=True)
        configure(variant)
        env=os.environ.copy()
        env.pop('ORDINARY_SOURCE',None)
        if variant=='clean': env['ORDINARY_SOURCE']=str(src)
        args=[d/'bin/ponyc','--pic','-b','work','-o',folder,src.parent]
        run([*args,'-r','ir'],env,folder/'ir.log')
        ir=(folder/'work.main.ll').read_text()
        assert ir.count('call void @ponyint_ordinary_poll(')==(64 if variant=='clean' else 0)
        run(args,env,folder/'compile.log')
        j=folder/'trace.json'
        j.unlink(missing_ok=True)
        p=subprocess.run([folder/'work','--ponymaxthreads=1','--ponytracingcategories=actor_behavior,actor_gc','--ponytracingoutput='+str(j)],capture_output=True,check=True)
        assert p.stdout==expected and not p.stderr
        events=json.loads(j.read_text())
        assert events[0]['name']=='start_trace' and events[-1]['name']=='end_trace'
        assert {'actor_behavior','actor_gc'} <= set(events[0]['args']['categories_enabled'])
        inside=None
        ticks=0
        for e in events:
            if e['name']=='actor_behavior_run' and e['args'].get('behavior_name')=='Worker.tick':
                if e['ph']=='b':
                    assert inside is None
                    inside=(e['id'],e['tid'])
                else:
                    assert e['ph']=='e' and inside==(e['id'],e['tid'])
                    ticks+=1
                    inside=None
            if inside and e['name']=='actor_gc':
                assert e['id']!=inside[0]
        assert ticks==16384 and inside is None
        print(variant,ticks,'completed behaviors, zero nested GC',flush=True)
    with (d/'cost.csv').open('w') as f:
        for rep in range(12):
            for v in (['stock','clean'] if rep%2==0 else ['clean','stock']):
                p=subprocess.run([d/'time-child',out/('cost-'+v)/'work','--ponymaxthreads=1'],capture_output=True,check=True)
                assert p.stdout==expected
                ns,rss=map(int,p.stderr.split())
                csv.writer(f).writerow([rep,v,ns,rss])
if __name__=='__main__':
    main()
