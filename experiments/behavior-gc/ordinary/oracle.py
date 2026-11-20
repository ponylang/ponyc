import os
import shutil
import subprocess
from pathlib import Path
from run import d,out,configure,run
def counter(case,needle,alter,label):
    folder=out/('oracle-'+label)
    source=folder/'source'
    source.mkdir(parents=True,exist_ok=True)
    for file in (d/case).glob('*.pony'):
        shutil.copyfile(file,source/file.name)
    f=source/'main.pony'
    data=f.read_text()
    assert data.count(needle)==1
    f.write_text(data.replace(needle,alter))
    env=os.environ.copy()
    env['ORDINARY_SOURCE']=str(f)
    run([d/'bin/ponyc','--pic','-p',Path(os.environ.get('PEG_DIR', Path.home()/'code/ponylang/peg')),'-b','work','-o',folder,source],env,folder/'compile.log')
    p=subprocess.run([folder/'work','small','--ponymaxthreads=1'],capture_output=True)
    assert p.returncode==1 and not p.stdout and not p.stderr,(p.returncode,p.stdout,p.stderr)
    print('oracle rejected',label,flush=True)
configure('clean')
counter('graph','(def.parameter is _shared)','(def.parameter isnt _shared)','binding')
counter('graph','(_expected * ((512','(_expected * ((513','checksum')
counter('regex','r.is_match("cd00")','r.is_match("cd0!")','acceptance')
counter('regex','r.is_match("ab17")','r.is_match("xxxx")','retained-regex')
counter('grammar','offset == src.content.size()','offset == (src.content.size() + 1)','consumption')
counter('grammar','(text == ','(text != ','golden')
counter('phase','((2 * count) + 1)','((2 * count) + 2)','state-count')
counter('phase','not compiled.is_match(wrong)','compiled.is_match(wrong)','negative-input')
