import csv
import itertools
import json
from pathlib import Path
import subprocess
import sys

here=Path(__file__).resolve().parent
sys.path.insert(0,str(here.parent))
from build import build
from run import compile_fixture, native_calls, native_stock, ordered

ROUNDS=65536
OPTIONS=['--ponymaxthreads=1','--ponynoscale','--ponygcinitial=30']


def checksum(n):
    mix=1
    value=0
    mask=(1<<64)-1
    for i in range(n):
        mix=(mix*17+i+1)&mask
        value=(value+i+3+max(0,i-1)+mix)&mask
    return value


def output(exe,expected,args):
    value,elapsed=map(int,subprocess.check_output([exe,str(ROUNDS),*OPTIONS,*args],text=True).split())
    if value!=expected or elapsed<=0:
        raise ValueError((value,elapsed))
    return elapsed


def verify_trace(path):
    count=0
    running=False
    actor=None
    first=last=None
    gcs=[]
    with path.open() as f:
        for line in f:
            raw=line.strip().strip(',').strip()
            if raw in ('[',']',''):
                continue
            event=json.loads(raw)
            name=event['name']
            if first is None:
                first=name
            last=name
            if name=='actor_behavior_run' and event['args']['behavior_name']=='Probe.measure':
                if actor not in (None,event['id']):
                    raise ValueError('multiple measured actors')
                actor=event['id']
                if event['ph']=='b' and not running:
                    running=True
                elif event['ph']=='e' and running:
                    running=False
                    count+=1
                else:
                    raise ValueError('behavior endpoints')
            if name=='actor_gc' and event['args']['type_name']=='Probe':
                gcs.append((count,running,event['ph']))
    if first!='start_trace' or last!='end_trace' or running or count!=ROUNDS:
        raise ValueError((first,last,count))
    if any(active or 0<idx<ROUNDS for idx,active,_ in gcs):
        raise ValueError('mid-interval GC: '+str(gcs))


def check_calls(exe, kind):
    middle = 'Probe_ref_middle_o'
    phase = 'Probe_ref_phase_o'
    mix = 'Probe_ref_mix_o'
    finish = 'Probe_ref_finish_o'
    bodies = [native_calls(exe, n) for n in ('Probe_Dispatch', 'Probe_tag_measure_o')]
    outer, = [body for body in bodies if middle in body]
    inner = native_calls(exe, middle)
    deepest = native_calls(exe, phase)
    helpers = [native_calls(exe, name) for name in (mix, finish)]
    methods = [outer, inner, deepest, *helpers]
    events = [[call for call in method if call.startswith('ponyint_frame_')
               or call == 'ponyint_threshold_sample'] for method in methods]
    if kind == 'full':
        push, pop = 'ponyint_frame_push', 'ponyint_frame_pop'
        claim, poll = 'ponyint_frame_claim', 'ponyint_frame_poll'
        expected = [[push, pop], [claim, push, pop], [claim, poll], [claim], [claim]]
        ordered(outer, [push, middle, pop])
        ordered(inner, [claim, push, phase, pop])
        ordered(deepest, [claim, mix, poll, finish])
    else:
        poll = ['ponyint_threshold_sample'] if kind == 'threshold' else []
        expected = [[], [], poll, [], []]
        ordered(inner, [phase])
        ordered(deepest, [mix, *poll, finish])
    if events != expected:
        raise ValueError((kind, events))


def main(full,out):
    out.mkdir()
    threshold=out/'threshold-build'
    build(threshold,threshold_only=True)
    stock=native_stock(out/'stock-build')
    variants=['stock','threshold','full']
    bins={}
    expected=checksum(ROUNDS)
    for kind in variants:
        exe=compile_fixture(threshold if kind=='threshold' else full,here/'fixture',
                            out/kind,enabled=kind!='stock',stock=stock)
        check_calls(exe,kind)
        trace=exe.parent/'trace.json'
        output(exe,expected,['--ponytracingcategories=actor_behavior,actor_gc','--ponytracingoutput='+str(trace)])
        verify_trace(trace)
        bins[kind]=exe
    dbg=out/'depth.gdb'
    dbg.write_text('''set pagination off
set confirm off
break ponyint_frame_poll
run 65536 --ponymaxthreads=1 --ponynoscale --ponygcinitial=30
printf "DEPTH=%u PERMIT=%u DISTINCT=%d PENDING=%d ACTOR=%d\\n", frames.depth, permit, frames.roots[0] != frames.roots[1], frames.pending != 0, frames.actor == actor
disable 1
continue
''')
    report=subprocess.check_output(['gdb','-nx','-batch','-x',dbg,bins['full']],text=True,stderr=subprocess.STDOUT)
    if 'DEPTH=2 PERMIT=2 DISTINCT=1 PENDING=0 ACTOR=1' not in report or 'exited normally' not in report:
        raise ValueError(report)
    (out/'depth.log').write_text(report)
    orders=list(itertools.permutations(variants))
    for exe in bins.values():
        output(exe,expected,[])
    with (out/'micro.csv').open('w',newline='') as f:
        writer=csv.writer(f,lineterminator='\n')
        writer.writerow(['pair','order','variant','behaviors','elapsed_ns','checksum'])
        for i in range(24):
            for order,kind in enumerate(orders[i%len(orders)]):
                writer.writerow([i,order,kind,ROUNDS,output(bins[kind],expected,[]),expected])


if __name__=='__main__':
    main(*(Path(arg).resolve() for arg in sys.argv[1:]))
