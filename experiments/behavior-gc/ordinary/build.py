if not __debug__:
    raise SystemExit("Experiment verification requires Python assertions enabled")

import os
import time
import shlex
import shutil
import subprocess
from pathlib import Path

d = Path(__file__).resolve().parent
r = Path.cwd()
build = r / 'build/build_debug/src'
def once(s, old, new):
    assert s.count(old) == 1, old
    return s.replace(old, new)
def compile_file(lib, sub, name, mode, contents, *extra):
    out = d / mode
    out.mkdir(exist_ok=True)
    path = out / name
    path.write_text(contents)
    flags = dict(line.split(' = ', 1) for line in
        (build/lib/'CMakeFiles'/f'{lib}.dir/flags.make').read_text().splitlines() if ' = ' in line)
    obj = out / (name + '.o')
    cmd = ['/usr/sbin/clang']
    for key in ('C_DEFINES','C_INCLUDES','C_FLAGS'):
        cmd.extend(shlex.split(flags[key]))
    subprocess.run([*cmd, '-I', r/'src'/lib/sub, *extra, '-c', path, '-o', obj], check=True)
    return obj
def archive(stock, target, objects):
    shutil.copyfile(stock,target)
    subprocess.run(['ar','rcs',target,*objects],check=True)
b = d/'bin'
b.mkdir(exist_ok=True)
s = (r/'src/libponyc/codegen/gencontrol.c').read_text()
s = once(s, 'LLVMValueRef gen_seq(', (d/'codegen.inc').read_text()+'\nLLVMValueRef gen_seq(')
s = once(s,'  ast_t* previous = NULL;', '  bool ordinary = ordinary_body(c, ast);\n  ast_t* previous = NULL;')
s = once(s,'    if(c->root_audit != NULL)','    if(ordinary && previous != NULL)\n      ordinary_emit(c, child);\n\n    if(c->root_audit != NULL)')
obj = compile_file('libponyc','codegen','gencontrol.c','compiler',s)
a = d/'compiler/libponyc.a'
archive(build/'libponyc/libponyc.a', a, [obj])
link = shlex.split((build/'ponyc/CMakeFiles/ponyc.dir/link.txt').read_text())
link[link.index('-o')+1] = str(b/'ponyc')
link[link.index('../libponyc/libponyc.a')] = str(a)
link = [f'--dependency-file={d}/compiler/link.d' if x.startswith('--dependency-file=') else x for x in link]
subprocess.run(link,cwd=build/'ponyc',check=True)
for mode in ['observed','clean']:
    s = (r/'src/libponyrt/actor/actor.c').read_text()
    s = once(s,'static void maybe_unblock(', (d/'actor.inc').read_text()+'\nstatic void maybe_unblock(')
    objects = []
    if mode == 'observed':
        s = once(s,'static void try_gc(', 'static _Thread_local bool ordinary_collecting;\nstatic _Thread_local int ordinary_completed;\nstatic void try_gc(')
        s = once(s,'  TRACING_ACTOR_GC_END(actor);','  TRACING_ACTOR_GC_END(actor);\n  if(ordinary_collecting) ordinary_completed++;')
        h = (r/'src/libponyrt/mem/heap.c').read_text()
        h = once(h,'void* ponyint_heap_alloc_small(', (d/'heap.inc').read_text()+'\nvoid* ponyint_heap_alloc_small(')
        h = once(h,'      n = heap->recyclable[HEAP_RECYCLE_MIXED];','      if(heap == ordinary_watched) ordinary_reused++;\n      n = heap->recyclable[HEAP_RECYCLE_MIXED];')
        h = once(h,'  // If there is no suitable one in the recyclable list, get a new one.', '  if(chunk != NULL && heap == ordinary_watched) ordinary_reused++;\n  // If there is no suitable one in the recyclable list, get a new one.')
        objects.append(compile_file('libponyrt','mem','heap.c',mode,h))
    objects.append(compile_file('libponyrt','actor','actor.c',mode,s,*(['-DORDINARY_OBSERVE'] if mode=='observed' else [])))
    archive(r/'build/debug/libponyrt-pic.a', d/mode/'libponyrt-pic.a', objects)
for path in (r/'build/debug').glob('crt*.o'):
    if not (b/path.name).exists(): (b/path.name).symlink_to(path)
if not (d/'packages').exists(): (d/'packages').symlink_to(r/'packages')
