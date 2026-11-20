import json
import os
from pathlib import Path
import re
import shutil
import subprocess

here = Path(__file__).resolve().parent
PONYCC = 'a36285efb6eb9b3c3ab62778613781c7b7535420'
PEG = '810ff6068018a96486a85a2d46e98ceabdeb3ef5'
TINY = '// café\nprimitive P\n'.encode()
INNER = b'let n = 42\n'


def checkout(key, revision):
    path = Path(os.environ[key]).resolve()
    if subprocess.check_output(['git','-C',path,'rev-parse','HEAD'],text=True).strip() != revision:
        raise ValueError(key+' revision')
    if subprocess.check_output(['git','-C',path,'status','--porcelain','--untracked-files=normal']):
        raise ValueError(key+' has local changes')
    return path


def literal(raw):
    return json.dumps(raw.decode('utf-8'), ensure_ascii=False)


def fixture(dest, modules, rounds=1, variant='enabled'):
    ponycc = checkout('PONYCC_DIR', PONYCC)
    peg = checkout('PEG_DIR', PEG)
    dest.mkdir()
    (dest / (dest.name.replace('-', '_') + '.pony')).write_text(
        '"""\nLex Pony source while two callers retain their token arrays.\n"""\n')
    names = set()
    for f in ['_lexer.pony','_lexicon.pony']:
        text = (ponycc/'ponycc/pass/parse'/f).read_text()
        names.update(re.findall(r'Tk\[(\w+)\]',text))
        text = re.sub(r'Tk\[(\w+)\]',r'_Label\1',text).replace('use "../../ast"','')
        (dest/f).write_text(text)
    labels = ['use peg = "peg"', 'type _Token is (TkAny, SourcePosAny)',
              'trait val TkAny is peg.Label']
    labels.extend(f'primitive _Label{n} is TkAny\n  fun text(): String => "{n}"' for n in sorted(names))
    (dest/'labels.pony').write_text('\n\n'.join(labels)+'\n')
    shutil.copy2(ponycc/'LICENSE', dest/'LICENSE.ponycc')
    positions = (ponycc/'ponycc/ast/source.pony').read_text()
    (dest/'source.pony').write_text(positions.replace('in Range(0,', 'in Range[USize].till(0,'))
    files = dict(modules)
    lines = ['primitive Inputs',
             '  fun outer(): Source => Source('+literal(TINY)+', "outer.pony")',
             '  fun inner(): Source => Source('+literal(INNER)+', "inner.pony")',
             '  fun unit(n: USize): Source =>',
             f'    match n % {len(files)}']
    for i,(path,data) in enumerate(files.items()):
        lines.append(f'    | {i} => Source('+literal(data)+', '+literal(path.encode())+')')
    lines += ['    else outer()', '    end']
    (dest/'inputs.pony').write_text('\n'.join(lines)+'\n')
    driver = (here/'driver.pony.in').read_text().replace('    lex_unit()\n', '    lex_unit()\n'*rounds)
    if variant == 'unarmed':
        driver = driver.replace('    let inner =', '    let padding: U64 = 0\n    let inner =')
    elif variant == 'finalizer':
        driver = driver.replace('  let _lexer:', '  let _keep: Resource = Resource\n  let _lexer:')
        driver += '\nclass Resource\n  fun _final() => None\n'
    (dest/'main.pony').write_text(driver)
    return ['-p',str(peg)]


def records(output):
    result = []
    at = 0
    while at < len(output):
        fields = []
        start = at
        for _ in range(4):
            stop = output.index(b'\t', start)
            fields.append(output[start:stop])
            start = stop + 1
        path, label, offset, length = fields
        end = start + int(length)
        if output[end:end+1] != b'\n':
            raise ValueError('token record length')
        result.append((path.decode(),label.decode(),int(offset),int(length),output[start:end]))
        at = end+1
    return result
