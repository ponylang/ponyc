import csv
import os
import shutil
import subprocess
from pathlib import Path
from run import configure, d, out, repo, run, trace

def fixture():
    path=d/'phase'
    path.mkdir(exist_ok=True)
    for f in (repo/'packages/iregex').glob('*.pony'):
        if f.name in ['_test.pony','iregex.pony']: continue
        target=path/f.name
        shutil.copyfile(f,target)
        assert target.read_bytes()==f.read_bytes()
    src='''use @pony_exitcode[None](code: I32)
use @puts[I32](s: Pointer[U8] tag)

actor Main
  new create(env: Env) =>
    @pony_exitcode(1)
    Worker(try env.args(1)? == "large" else false end)

actor Worker
  let _large: Bool
  var _ast: (_RegexNode | None) = None
  var _result: (_NFA | None) = None

  new create(large: Bool) =>
    _large = large
    if large then run_large() else run_small() end

  be run_small() =>
    parse_input()
    build_nfa()
    verify()

  be run_large() =>
    parse_input()
    build_nfa()
    verify()

  fun ref parse_input() =>
    let pattern = if _large then "PL" else "PS" end
    try _ast = _IRegexpParser(pattern).parse()? end

  fun ref build_nfa() =>
    try
      _result = _NFABuilder.build(_ast as _RegexNode)?
    end
    _ast = None

  fun verify() =>
    let input = if _large then "IL" else "IS" end
    let wrong = if _large then "WL" else "WS" end
    let count: USize = if _large then 256 else 64 end
    match _result
    | let nfa: _NFA =>
      let compiled = IRegexp._create(nfa)
      if (nfa.num_states == ((2 * count) + 1))
        and compiled.is_match(input) and (not compiled.is_match(wrong))
        and (not compiled.is_match(input + "a"))
      then
        @puts("verified".cstring())
        @pony_exitcode(0)
      end
    end
'''
    for key,n in [('S',64),('L',256)]:
        src=src.replace('"P'+key+'"','"'+'(ab)'*n+'"').replace('"I'+key+'"','"'+'ab'*n+'"').replace('"W'+key+'"','"'+'ab'*(n-1)+'aa"')
    (path/'main.pony').write_text(src)
    return path/'main.pony'
def measure():
    src=fixture()
    for codegen,flags in [('debug',['-d']),('optimized',[])]:
        for variant in ['observed','clean','stock']:
            folder=out/f'phase-{codegen}-{variant}'
            folder.mkdir(exist_ok=True)
            configure(variant)
            env=os.environ.copy()
            env.pop('ORDINARY_SOURCE',None)
            env.pop('ORDINARY_SAMPLE_ONLY',None)
            if variant!='stock': env['ORDINARY_SOURCE']=str(src)
            args=[d/'bin/ponyc',*flags,'--pic','-o',folder,'-b','work',src.parent]
            run(args,env,folder/'compile.log')
            for scale in ['small','large']:
                idx=src.read_text().splitlines().index('  be run_'+scale+'() =>')
                for sample in ([False,True] if variant=='observed' else [False]):
                    child=env.copy()
                    if sample: child['ORDINARY_SAMPLE_ONLY']='1'
                    kind='sample' if sample else variant
                    j=folder/(scale+'-'+kind+'.json')
                    j.unlink(missing_ok=True)
                    p=subprocess.run([folder/'work',scale,'--ponymaxthreads=1',
                        '--ponytracingcategories=actor_behavior,actor_gc','--ponytracingoutput='+str(j)],env=child,capture_output=True,check=True)
                    j.with_suffix('.err').write_bytes(p.stderr)
                    assert p.stdout==b'verified\n'
                    addr,gcs=trace(j,scale)
                    if variant=='observed':
                        rows=[l.split() for l in p.stderr.decode().splitlines()]
                        assert len(rows)==2 and all(r[:2]==['poll',addr] for r in rows)
                        first,second=[list(map(int,r[2:])) for r in rows]
                        assert [first[0],second[0]]==[idx+3,idx+4]
                        assert gcs==first[4]+second[4]
                        assert all(r[3]==(r[1]>r[2]) and r[4]==(r[3] and not sample) for r in (first,second))
                        assert gcs==(0 if sample else 2)
                        assert second[8]>0 if not sample else second[8]==0
                        assert first[6]>0 and second[6]>=first[7] and first[8]==0
                        with (d/'phase.csv').open('a') as f:
                            csv.writer(f).writerow([codegen,scale,kind,first[1],first[5],first[6],first[7],second[6],second[8]])
                        print(codegen,scale,kind,first,second,flush=True)
                    else:
                        assert gcs==(2 if variant=='clean' else 0)
if __name__=='__main__':
    measure()
