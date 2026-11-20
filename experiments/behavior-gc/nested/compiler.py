import json
from pathlib import Path
import shlex
import shutil
import subprocess


def build_compiler(trial, config):
    repo = Path.cwd()
    build = config.resolve() / 'src'
    original = repo / 'src/libponyc/codegen/genfun.c'
    contents = original.read_text()
    anchor = '    call_embed_finalisers(c, t, body, gen_this(c, NULL));\n\n'
    anchor += '  LLVMValueRef value = gen_expr(c, body);'
    if contents.count(anchor) != 1:
        raise ValueError('expected ordinary method code generation')
    fixture = json.dumps(str(trial / 'fixture/main.pony'))
    contents = contents.replace(anchor, f'''    call_embed_finalisers(c, t, body, gen_this(c, NULL));

  source_t* source = ast_source(body);
  bool nested = c->opt->gc_root_audit && source != NULL && source->file != NULL &&
    strcmp(source->file, {fixture}) == 0 &&
    strcmp(m->full_name, "Probe_ref_middle_o") == 0;
  LLVMValueRef value = nested ? gen_root_audit_body(c, t, m, body) : gen_expr(c, body);''')
    obj = trial / 'objects/genfun.c.o'
    overlay = trial / 'objects/genfun.c'
    overlay.write_text(contents)
    flags = dict(line.split(' = ', 1) for line in
                 (build / 'libponyc/CMakeFiles/libponyc.dir/flags.make').read_text().splitlines()
                 if ' = ' in line)
    cc = ['/usr/sbin/clang']
    for key in ['C_DEFINES', 'C_INCLUDES', 'C_FLAGS']:
        cc.extend(shlex.split(flags[key]))
    subprocess.run([*cc, '-I', original.parent, '-c', overlay, '-o', obj], check=True)
    archive = trial / 'objects/libponyc.a'
    shutil.copyfile(build / 'libponyc/libponyc.a', archive)
    subprocess.run(['ar', 'rcs', archive, obj], check=True)
    link = shlex.split((build / 'ponyc/CMakeFiles/ponyc.dir/link.txt').read_text())
    link[link.index('../libponyc/libponyc.a')] = str(archive)
    link[link.index('-o') + 1] = str(trial / 'bin/ponyc')
    link = ['--dependency-file=' + str(trial / 'objects/link.d')
            if x.startswith('--dependency-file=') else x for x in link]
    subprocess.run(link, cwd=build / 'ponyc', check=True)
