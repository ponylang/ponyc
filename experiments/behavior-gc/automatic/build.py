import json
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


def object_file(out, lib, original, contents, *flags, flag_file=None):
    path = out / original.name
    path.write_text(contents)
    if flag_file is None:
        flag_file = config / lib / 'CMakeFiles' / (lib + '.dir/flags.make')
    variables = dict(line.split(' = ', 1) for line in flag_file.read_text().splitlines()
        if ' = ' in line)
    language = 'CXX' if original.suffix in ('.cc', '.cpp') else 'C'
    cmd = ['/usr/sbin/clang++' if language == 'CXX' else '/usr/sbin/clang']
    for key in [language + suffix for suffix in ('_DEFINES', '_INCLUDES', '_FLAGS')]:
        cmd.extend(shlex.split(variables[key]))
    obj = path.with_suffix(path.suffix + '.o')
    subprocess.run([*cmd, *flags, '-I', original.parent, '-c', path, '-o', obj], check=True)
    return obj


def archive(src, dst, obj):
    shutil.copyfile(src, dst)
    subprocess.run(['ar', 'rcs', dst, obj], check=True)


def build(out, threshold_only=False, *, threshold_first=False, stackmap=False):
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
    if threshold_first or stackmap:
        s = replace(s, 'LLVMBuildLoad2(c->builder, c->ptr, slot, "")', 'slot')
        s = replace(s, '  return gencall_runtime(c, name, args, (int)count, "");',
            '  LLVMValueRef r = gencall_runtime(c, name, args, (int)count, "");\n'
            '  LLVMSetMetadataStr(r, "pony.gc.frame", LLVMMDNodeInContext(c->context, NULL, 0));\n'
            '  return r;')
    flags = ['-DFRAME_THRESHOLD_ONLY'] if threshold_only else []
    obj = object_file(out, 'libponyc', path, s, *flags)
    lib = out / 'libponyc.a'
    archive(config/'libponyc/libponyc.a', lib, obj)
    if threshold_first or stackmap:
        path = repo / 'src/libponyc/codegen/genopt.cc'
        opt = replace(path.read_text(), '#include <llvm/IR/Module.h>',
            '#include <llvm/IR/Module.h>\n#include <llvm/Analysis/ValueTracking.h>\n'
            '#include <llvm/Transforms/Utils/BasicBlockUtils.h>\n'
            '#include <llvm/Transforms/Utils/Cloning.h>')
        opt = replace(opt, 'static bool pony_opt_module(',
            (here/'markers.inc').read_text() +
            (here/('stackmap' if stackmap else 'threshold')/'lower.inc').read_text() +
            '\nstatic bool pony_opt_module(')
        opt = replace(opt, '  LLVMDIBuilderFinalize(di);',
            '  LLVMDIBuilderFinalize(di);\n  lowerThresholdPolls(*unwrap(module));')
        obj = object_file(out, 'libponyc', path, opt)
        subprocess.run(['ar', 'rcs', lib, obj], check=True)
    if stackmap:
        script = here/'stackmap/maps.ld'
        path = repo/'src/libponyc/codegen/genexe.cc'
        contents = replace(path.read_text(), '  args.push_back("relro");',
            '  args.push_back("relro");\n  args.push_back("-T");\n'
            + '  args.push_back(' + json.dumps(str(script)) + ');')
        obj = object_file(out, 'libponyc', path, contents)
        subprocess.run(['ar', 'rcs', lib, obj], check=True)
    link = shlex.split((config/'ponyc/CMakeFiles/ponyc.dir/link.txt').read_text())
    if stackmap:
        path = repo/'lib/llvm/src/llvm/lib/MC/MCObjectFileInfo.cpp'
        contents = replace(path.read_text(), '".llvm_stackmaps", ELF::SHT_PROGBITS, ELF::SHF_ALLOC',
            '".llvm_stackmaps", ELF::SHT_PROGBITS, ELF::SHF_ALLOC | ELF::SHF_WRITE')
        flags_path = repo/'build/build_libs/llvm/src/llvm/lib/MC/CMakeFiles/LLVMMC.dir/flags.make'
        obj = object_file(out, 'LLVMMC', path, contents, flag_file=flags_path)
        original = repo/'build/libs/lib/libLLVMMC.a'
        archive(original, out/original.name, obj)
        link[link.index(str(original))] = str(out/original.name)
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
    extra = (here/'threshold/runtime.inc').read_text() if threshold_first or stackmap else ''
    if stackmap:
        extra += (here/'stackmap/runtime.inc').read_text()
    runtime = s[:start] + gc + (here/'runtime.inc').read_text() + extra + '\n' + s[end:]
    obj = object_file(out, 'libponyrt', path, runtime, *flags)
    archive(repo/'build/debug/libponyrt-pic.a', out/'bin/libponyrt-pic.a', obj)


if __name__ == '__main__':
    build(Path(sys.argv[1]).resolve())
