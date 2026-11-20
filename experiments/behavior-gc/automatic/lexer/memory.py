import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

from workload import fixture, records, TINY, INNER

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run import compile_fixture, native_stock

here = Path(__file__).resolve().parent
modules = ['packages/builtin/compare.pony', 'packages/builtin/nullable_pointer.pony',
           'packages/collections/range.pony', 'packages/collections/ring_buffer.pony']


def cycle_count(trace, behavior='Probe.measure'):
    events = json.loads(trace.read_text())
    if events[0]['name'] != 'start_trace' or events[-1]['name'] != 'end_trace':
        raise ValueError('incomplete trace')
    bounds = [(i,e) for i,e in enumerate(events) if e['name']=='actor_behavior_run'
              and e['args']['behavior_name']==behavior]
    (begin,b),(end,e) = bounds
    if (b['ph'],e['ph']) != ('b','e') or (b['tid'],b['id']) != (e['tid'],e['id']):
        raise ValueError('mismatched behavior endpoints')
    cycles = [x for x in events[begin+1:end] if x['name']=='actor_gc' and x['id']==b['id']]
    if len(cycles)%2 or [x['ph'] for x in cycles] != ['b','e']*(len(cycles)//2):
        raise ValueError('incomplete GC cycle')
    if any(x['tid']!=b['tid'] or not (b['ts']<=x['ts']<=e['ts']) for x in cycles):
        raise ValueError('GC outside measured behavior')
    return len(cycles)//2,b['args']['behavior_id']


def boundary_charges(exe, args, behavior_id, directory):
    commands = directory / 'charge.gdb'
    commands.write_text(f'''set pagination off
set confirm off
break ponyint_tracing_actor_behavior_run_start if behavior_id == {behavior_id} && $_streq(actor->type->name, "Probe")
commands
silent
printf "START_CHARGE=%zu\\n", actor->heap.used
continue
end
break ponyint_tracing_actor_behavior_run_end if behavior_id == {behavior_id} && $_streq(actor->type->name, "Probe")
run {' '.join(args)} --ponytracingcategories=actor_behavior --ponytracingoutput=charge-trace.json > /dev/null
printf "END_CHARGE=%zu\\n", actor->heap.used
continue
''')
    r = subprocess.check_output(['gdb','-nx','-batch','-x',commands,exe],
                                text=True,stderr=subprocess.STDOUT,cwd=directory)
    (directory/'debugger.log').write_text(r)
    n, = re.findall(r'^END_CHARGE=(\d+)$',r,re.M)
    if 'exited normally' not in r:
        raise ValueError(r)
    start, = re.findall(r'^START_CHARGE=(\d+)$',r,re.M)
    return int(start),int(n)


def write_csv(path, rows):
    with path.open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=rows[0],lineterminator='\n')
        w.writeheader()
        w.writerows(rows)


def validate_tokens(data, inputs, rounds):
    tokens = records(data)
    sources = {**inputs,'outer.pony':TINY,'inner.pony':INNER}
    paths = []
    prev_offset = 0
    for path,label,start,length,word in tokens:
        if not paths or paths[-1]!=path:
            paths.append(path)
            prev_offset=0
        if not label or length<=0 or start<prev_offset or sources[path][start:start+length]!=word:
            raise ValueError((path,label,start,length,word))
        prev_offset=start+length
    if paths != (list(inputs)*(rounds//len(inputs)))+['outer.pony','inner.pony']:
        raise ValueError('unexpected source file order')
    return len(tokens)


def main(build,out):
    out.mkdir()
    stock = native_stock(out / 'native-stock')
    inputs = {path:Path(path).read_bytes() for path in modules}
    timer=out/'time-child'
    subprocess.run(['cc','-O2','-Wall','-Werror',here.parents[1]/'ordinary/time-child.c',
                    '-o',timer],check=True)
    measurements=[]
    times=[]
    single_pass=None
    for rounds in [4,16]:
        src=out/f'src-{rounds}'
        flags=fixture(src,inputs.items(),rounds)
        bins={}
        reference=None
        for variant in ['stock','enabled','high','stock-high']:
            directory=out/f'{rounds}-{variant}'
            if variant in ('high','stock-high'):
                directory.mkdir()
                exe=bins['enabled' if variant=='high' else 'stock']
            else:
                exe=compile_fixture(build,src,directory,enabled=variant!='stock',extra=flags,stock=stock)
                if 'Merging runtime' in (directory/'compile.log').read_text():
                    raise ValueError('expected native runtime')
            bins[variant]=exe
            args=['--ponymaxthreads=1','--ponynoscale']
            if variant in ('high','stock-high'):
                args.append('--ponygcinitial=30')
            trace=directory/'trace.json'
            data=subprocess.check_output([exe,*args,'--ponytracingcategories=actor_gc,actor_behavior',
                                           '--ponytracingoutput='+str(trace)])
            ntokens=validate_tokens(data,inputs,rounds)
            if reference is None:
                reference=data
            if data!=reference:
                raise ValueError('compiled token streams differ')
            if rounds==4:
                single_pass=records(data)[:-9]
            elif records(data)[:-9]!=single_pass*4:
                raise ValueError('scaled output differs from four complete passes')
            cycles,ident=cycle_count(trace)
            if cycles != (rounds if variant=='enabled' else 0):
                raise ValueError((rounds,variant,cycles))
            start,used=boundary_charges(exe,args,ident,directory)
            digest=hashlib.sha256(data).hexdigest()
            measurements.append(dict(modules=rounds,variant=variant,source_bytes=sum(map(len,inputs.values()))*rounds//4,
                                     tokens=ntokens,cycles=cycles,start_charge=start,end_charge=used,sha256=digest))
        for rep in range(7):
            order=['stock','enabled','high','stock-high'] if rep%2==0 else ['stock-high','high','enabled','stock']
            for variant in order:
                cmd=[timer,bins[variant],'--ponymaxthreads=1','--ponynoscale']
                if variant in ('high','stock-high'):
                    cmd.append('--ponygcinitial=30')
                r=subprocess.run(cmd,capture_output=True,check=True)
                if r.stdout!=reference:
                    raise ValueError('timed token stream differs')
                ns,kib=map(int,r.stderr.split())
                times.append(dict(modules=rounds,rep=rep,variant=variant,elapsed_ns=ns,peak_rss_kib=kib))
        write_csv(out/'memory.csv',measurements)
        write_csv(out/'timing.csv',times)
        print(measurements[-4:],flush=True)


if __name__=='__main__':
    main(*(Path(a).resolve() for a in sys.argv[1:]))
