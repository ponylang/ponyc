import argparse
import shlex
import shutil
import subprocess
from pathlib import Path

repo = Path.cwd()
parser = argparse.ArgumentParser()
parser.add_argument('--trial', type=Path, default=Path(__file__).resolve().parent)
parser.add_argument('--site-marker', default='let replacement =')
parser.add_argument('--site-token', default='=')
options = parser.parse_args()
trial = options.trial.resolve()
runtime = repo / 'src/libponyrt'
binary = trial / 'bin'
objects = trial / 'objects'
if not (runtime / 'actor/actor.c').is_file():
    raise SystemExit('Run the experiment from the configured ponyc checkout root')
binary.mkdir(exist_ok=True)
objects.mkdir(exist_ok=True)
source = (runtime / 'actor/actor.c').read_text()
begin = source.index('static void try_gc(')
end = source.index('static void maybe_unblock', begin)
original = source[begin:end]
def once(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError(f'unexpected source shape: {old}')
    return text.replace(old, new)
variant = once(original, 'static void try_gc(pony_ctx_t* ctx, pony_actor_t* actor)',
    'bool probe_collect_roots(pony_ctx_t* ctx, pony_actor_t* actor, size_t count, '
    'void* const* roots, void (*check_marks)(void))')
variant = once(variant, '    return;', '    return false;')
variant = once(variant, '  ponyint_mark_done(ctx);',
    '  for(size_t i = 0; i < count; i++)\n'
    '    pony_traceunknown(ctx, roots[i], PONY_TRACE_MUTABLE);\n\n'
    '  ponyint_mark_done(ctx);\n  if(check_marks != NULL) check_marks();')
variant = once(variant, '  TRACING_ACTOR_GC_END(actor);',
    '  TRACING_ACTOR_GC_END(actor);\n  return true;')
variant += '''bool probe_collect(pony_ctx_t* ctx, pony_actor_t* actor, void* leaf)
{
  return probe_collect_roots(ctx, actor, leaf != NULL ? 1 : 0, &leaf, NULL);
}

static void try_gc(pony_ctx_t* ctx, pony_actor_t* actor)
{
  (void)probe_collect(ctx, actor, NULL);
}

'''
(objects / 'actor.c').write_text(source[:begin] + variant + source[end:])
flags_path = repo / 'build/build_debug/src/libponyrt/CMakeFiles/libponyrt.dir/flags.make'
flags = {}
for line in flags_path.read_text().splitlines():
    if ' = ' in line:
        key, value = line.split(' = ', 1)
        flags[key] = shlex.split(value)
cc = ['/usr/sbin/clang', *flags['C_DEFINES'], *flags['C_INCLUDES'], *flags['C_FLAGS']]
site, = [(idx, line.index(options.site_token) + 1) for idx, line in enumerate(
    (trial / 'fixture/main.pony').read_text().splitlines(), 1) if options.site_marker in line]
subprocess.run([*cc, '-I', runtime / 'actor', '-c', objects / 'actor.c',
                '-o', objects / 'actor.c.o'], check=True)
subprocess.run([*cc, f'-DSITE_LINE={site[0]}', f'-DSITE_COLUMN={site[1]}',
                '-c', trial / 'probe.c', '-o', objects / 'root_audit.c.o'], check=True)
archive = binary / 'libponyrt-pic.a'
shutil.copyfile(repo / 'build/debug/libponyrt-pic.a', archive)
subprocess.run(['ar', 'rcs', archive, objects / 'actor.c.o', objects / 'root_audit.c.o'], check=True)
shutil.copy2(repo / 'build/debug/ponyc', binary / 'ponyc')
for path in (repo / 'build/debug').glob('crt*.o'):
    (binary / path.name).symlink_to(path) if not (binary / path.name).exists() else None
if not (trial / 'packages').exists():
    (trial / 'packages').symlink_to(repo / 'packages')
