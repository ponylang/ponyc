import csv
import hashlib
import json
from pathlib import Path
import sys
import subprocess

from workload import fixture, records, TINY, INNER

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from run import compile_fixture, check_trace


def golden(stream):
    first = [('NewLine',8,b'\n'), ('Primitive',9,b'primitive'), ('Id',19,b'P'), ('NewLine',20,b'\n')]
    second = [('Let',0,b'let'),('Id',4,b'n'),('Assign',6,b'='),('LitInteger',8,b'42'),('NewLine',10,b'\n')]
    expected = [(path, label, offset, len(text), text)
                for path, tokens in [('unit.pony',first),('outer.pony',first),('inner.pony',second)]
                for label, offset, text in tokens]
    if records(stream) != expected:
        raise ValueError((records(stream),expected))


def main(build, out):
    out.mkdir()
    rows = []
    reference = None
    for variant in ['stock','enabled','unarmed','finalizer']:
        src = out / (variant+'-src')
        args = fixture(src,[('unit.pony',TINY)], variant=variant)
        for codegen in ['optimized','debug']:
            exe = compile_fixture(build,src,out/(variant+'-'+codegen),codegen=='debug',variant!='stock',args)
            for threads in [1,2]:
                trace = exe.parent / f'{threads}.json'
                stream = subprocess.check_output([exe,f'--ponymaxthreads={threads}','--ponynoscale',
                    '--ponytracingcategories=actor_gc,actor_behavior','--ponytracingoutput='+str(trace)])
                golden(stream)
                if reference is None:
                    reference=stream
                if reference != stream:
                    raise ValueError('different token stream')
                cycles=check_trace(json.loads(trace.read_text()),variant=='enabled')
                rows.append([variant,codegen,threads,cycles,len(records(stream)),hashlib.sha256(stream).hexdigest()])
    for old, new in [(b'Primitive',b'Interface'),(b'\t19\t',b'\t18\t'),(b'inner.pony',b'outer.pony'),(b'\t42\n',b'\t43\n')]:
        try:
            golden(reference.replace(old,new))
        except ValueError:
            continue
        raise ValueError('accepted corrupted token stream')
    (out/'golden.tokens').write_bytes(reference)
    with (out/'runs.csv').open('w', newline='') as f:
        w=csv.writer(f,lineterminator='\n')
        w.writerow(['variant','codegen','threads','collections','tokens','sha256'])
        w.writerows(rows)


if __name__ == '__main__':
    main(*(Path(arg).resolve() for arg in sys.argv[1:]))
