if not __debug__:
    raise SystemExit("Experiment verification requires Python assertions enabled")

import csv
import json
import os
import re
import shutil
import statistics
import subprocess
import time
from pathlib import Path

d = Path(__file__).resolve().parent
repo = Path.cwd()
out = d/'artifacts'
out.mkdir(exist_ok=True)
logs = d/'logs'
peg = Path(os.environ.get('PEG_DIR', Path.home()/'code/ponylang/peg'))
def run(args, env, logfile, **kw):
    with logfile.open('w') as f:
        subprocess.run(args, env=env, stdout=f, stderr=subprocess.STDOUT, check=True, **kw)
def configure(mode):
    dest = d/'bin/libponyrt-pic.a'
    src = repo/'build/debug/libponyrt-pic.a' if mode=='stock' else d/mode/'libponyrt-pic.a'
    dest.unlink(missing_ok=True)
    dest.symlink_to(src)
def trace(path, scale):
    events = json.loads(path.read_text())
    assert events[0]['name']=='start_trace' and events[-1]['name']=='end_trace'
    assert {'actor_behavior','actor_gc'} <= set(events[0]['args']['categories_enabled'])
    endpoints = [(i,e) for i,e in enumerate(events) if e['name']=='actor_behavior_run'
                 and e['args'].get('behavior_name')=='Worker.run_'+scale]
    assert [e['ph'] for _,e in endpoints]==['b','e'], endpoints
    (first,begin),(last,end) = endpoints
    assert begin['id']==end['id'] and begin['tid']==end['tid']
    phases = [e for e in events[first+1:last] if e['name']=='actor_gc' and e['id']==begin['id']]
    assert [e['ph'] for e in phases]==['b','e']*(len(phases)//2)
    assert all(p['tid']==begin['tid'] for p in phases)
    times = [begin['ts'],*[p['ts'] for p in phases],end['ts']]
    assert times==sorted(times)
    return begin['id'],len(phases)//2
def records(path,addr,sites,enabled,gcs):
    rows=[]
    for line in path.read_text().splitlines():
        parts=line.split()
        assert len(parts)==11 and parts[:2]==['poll',addr], parts
        rows.append(tuple(map(int,parts[2:])))
    assert [row[0] for row in rows]==sites, (sites[:4],rows[:3])
    assert sum(r[4] for r in rows)==gcs
    assert all(r[3]==int(r[1]>r[2]) for r in rows)
    assert all(r[4]==(r[3] if enabled else 0) for r in rows)
    assert all((r[5]<r[1] if r[4] else r[5]==r[1]) for r in rows)
    assert all((not r[1] or r[6]>0) and (not r[5] or r[7]>0) for r in rows)
    assert all(r[7]==r[6] for r in rows if not r[4])
    reused=sum(r[8] for r in rows)
    assert bool(reused)==enabled
    completed=False
    for r in rows:
        assert not r[8] or completed
        completed |= bool(r[4])
    return [max(max(r[6:8]) for r in rows),max(r[1] for r in rows),rows[-1][5],reused]
def collect_matrix(case):
    base=os.environ.copy()
    for key in ['ORDINARY_SOURCE','ORDINARY_SAMPLE_ONLY','PONY_THIN_LTO','PONY_DEBUG']:
        base.pop(key,None)
    src=d/case/'main.pony'
    lines=src.read_text().splitlines()
    sites={}
    for scale,n in [('small',8),('large',64)]:
        idx=lines.index('  be run_'+scale+'() =>')
        sites[scale]=list(range(idx+3,idx+n+4))
    expected=set(sites['small']+sites['large'])
    for codegen,flags in [('debug',['-d']),('optimized',[])]:
        for variant in ['observed','clean','stock']:
            folder=out/f'{case}-{codegen}-{variant}'
            folder.mkdir(exist_ok=True)
            configure(variant)
            env=base.copy()
            if variant!='stock': env['ORDINARY_SOURCE']=str(src)
            args=[d/'bin/ponyc',*flags,'--pic','-p',peg,'-o',folder,'-b','work',d/case]
            if variant!='clean':
                run([*args,'-r','ir'],env,folder/'ir.log')
                ir=(folder/'work.main.ll').read_text()
                calls = re.findall(r'\bcall void @ponyint_ordinary_poll\([^\n]*?i32(?: [a-z]+)* (\d+)\)',ir)
                assert set(map(int,calls))==(expected if variant=='observed' else set()), (case,codegen,calls[:5])
                if variant=='observed':
                    declaration=next(l for l in ir.splitlines() if l.startswith('declare void @ponyint_ordinary_poll('))
                    assert re.fullmatch(r'declare void @ponyint_ordinary_poll\(ptr, ptr, i32\)', declaration), declaration
            run(args,env,folder/'compile.log')
            for scale,n in [('small',8),('large',64)]:
                for sample in ([False,True] if variant=='observed' else [False]):
                    kind='sample' if sample else variant
                    logfile=folder/(scale+'-'+kind)
                    child=base.copy()
                    if sample: child['ORDINARY_SAMPLE_ONLY']='1'
                    j=logfile.with_suffix('.json')
                    j.unlink(missing_ok=True)
                    with logfile.with_suffix('.out').open('w') as stdout, logfile.with_suffix('.err').open('w') as stderr:
                        subprocess.run([folder/'work',scale,'--ponymaxthreads=1','--ponytracingcategories=actor_behavior,actor_gc',
                                        '--ponytracingoutput='+str(j)],stdout=stdout,stderr=stderr,env=child,check=True)
                    assert logfile.with_suffix('.out').read_text().splitlines()==['verified']
                    addr,gcs=trace(j,scale)
                    enabled=kind in ['clean','observed']
                    assert gcs==(n if enabled else 0),(case,scale,kind,gcs,n)
                    metrics=records(logfile.with_suffix('.err'),addr,sites[scale],enabled,gcs) if variant=='observed' else ['']*4
                    with (d/'measurements.csv').open('a') as f:
                        csv.writer(f).writerow([case,codegen,scale,kind,gcs,*metrics])
                    print(case,codegen,scale,kind,gcs,metrics,flush=True)
def timings(case):
    base=os.environ.copy()
    for scale in ['small','large']:
        for high in [False,True]:
            for rep in range(5):
                variants=['clean','stock'] if rep%2==0 else ['stock','clean']
                for v in variants:
                    folder=out/f'{case}-optimized-{v}'
                    args=[folder/'work',scale,'--ponymaxthreads=1']
                    if high: args.append('--ponygcinitial=30')
                    with (d/'native-timings.csv').open('a') as f:
                        result=subprocess.run([d/'time-child',*args],capture_output=True,env=base,check=True)
                        assert result.stdout==b'verified\n'
                        ns,rss=map(int,result.stderr.split())
                        csv.writer(f).writerow([case,scale,high,rep,v,ns,rss])
if __name__=='__main__':
    import sys
    cases=sys.argv[1:] or ['graph','regex','grammar']
    for case in cases:
        collect_matrix(case)
        timings(case)
