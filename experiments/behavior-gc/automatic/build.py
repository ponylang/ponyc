from pathlib import Path
import shlex
import shutil
import subprocess
import sys

here = Path(__file__).resolve().parent
repo = Path.cwd()
config = repo / 'build/build_debug/src'


def replace(s, old, new):
    if s.count(old) != 1:
        raise ValueError(old)
    return s.replace(old, new)


def object_file(out, lib, original, contents, *flags):
    path = out / original.name
    path.write_text(contents)
    variables = dict(line.split(' = ', 1) for line in
        (config / lib / 'CMakeFiles' / (lib + '.dir/flags.make')).read_text().splitlines()
        if ' = ' in line)
    cmd = ['/usr/sbin/clang']
    for key in ['C_DEFINES', 'C_INCLUDES', 'C_FLAGS']:
        cmd.extend(shlex.split(variables[key]))
    obj = path.with_suffix('.c.o')
    subprocess.run([*cmd, *flags, '-I', original.parent, '-c', path, '-o', obj], check=True)
    return obj


def archive(src, dst, obj):
    shutil.copyfile(src, dst)
    subprocess.run(['ar', 'rcs', dst, obj], check=True)


def build(out, threshold_only=False):
    out.mkdir()
    (out / 'bin').mkdir()
    (out / 'packages').symlink_to(repo / 'packages')
    for path in (repo / 'build/debug').glob('crt*.o'):
        (out / 'bin' / path.name).symlink_to(path)
    path = repo / 'src/libponyc/codegen/gencontrol.c'
    s = replace(path.read_text(), 'LLVMValueRef gen_seq(', (here/'codegen.inc').read_text()+'\nLLVMValueRef gen_seq(')
    s = replace(s, '  ast_t* previous = NULL;', '  frame_site_t site = frame_sites(c, ast);\n  ast_t* previous = NULL;')
    s = replace(s, '    value = gen_expr(c, child);', '''    LLVMValueRef ticket = frame_before(c, &site, previous, child);
    value = gen_expr(c, child);
    if(ticket != NULL && value != NULL && value != GEN_NOVALUE)
      frame_call(c, "ponyint_frame_pop", &ticket, 1);''')
    flags = ['-DFRAME_THRESHOLD_ONLY'] if threshold_only else []
    obj = object_file(out, 'libponyc', path, s, *flags)
    lib = out / 'libponyc.a'
    archive(config/'libponyc/libponyc.a', lib, obj)
    link = shlex.split((config/'ponyc/CMakeFiles/ponyc.dir/link.txt').read_text())
    link[link.index('../libponyc/libponyc.a')] = str(lib)
    link[link.index('-o') + 1] = str(out/'bin/ponyc')
    link = ['--dependency-file='+str(out/'link.d') if x.startswith('--dependency-file=') else x for x in link]
    subprocess.run(link, cwd=config/'ponyc', check=True)
    path = repo / 'src/libponyrt/actor/actor.c'
    s = path.read_text()
    start = s.index('static void try_gc(')
    end = s.index('static void maybe_unblock', start)
    gc = s[start:end]
    gc = replace(gc, 'static void try_gc(pony_ctx_t* ctx, pony_actor_t* actor)',
                 'static bool frame_collect(pony_ctx_t* ctx, pony_actor_t* actor, void* const* roots, size_t count)')
    gc = replace(gc, '    return;', '    return false;')
    gc = replace(gc, '  ponyint_mark_done(ctx);', '''  for(size_t i = 0; i < count; i++)
    pony_traceunknown(ctx, roots[i], PONY_TRACE_MUTABLE);
  ponyint_mark_done(ctx);''')
    gc = replace(gc, '  TRACING_ACTOR_GC_END(actor);', '  TRACING_ACTOR_GC_END(actor);\n  return true;')
    gc += '''static void try_gc(pony_ctx_t* ctx, pony_actor_t* actor)
{
  (void)frame_collect(ctx, actor, NULL, 0);
}
'''
    runtime = s[:start] + gc + (here/'runtime.inc').read_text() + '\n' + s[end:]
    obj = object_file(out, 'libponyrt', path, runtime, *flags)
    archive(repo/'build/debug/libponyrt-pic.a', out/'bin/libponyrt-pic.a', obj)


if __name__ == '__main__':
    build(Path(sys.argv[1]).resolve())
