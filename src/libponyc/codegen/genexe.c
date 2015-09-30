#include "genexe.h"
#include "genopt.h"
#include "genobj.h"
#include "gentype.h"
#include "genfun.h"
#include "gencall.h"
#include "genname.h"
#include "genprim.h"
#include "../reach/paint.h"
#include "../pkg/package.h"
#include "../pkg/program.h"
#include "../type/assemble.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <assert.h>

#ifdef PLATFORM_IS_POSIX_BASED
#  include <unistd.h>
#endif

#if defined(PLATFORM_IS_LINUX) || defined(PLATFORM_IS_FREEBSD)

static bool file_exists(const char* filename)
{
  struct stat s;
  int err = stat(filename, &s);

  return (err != -1) && S_ISREG(s.st_mode);
}

static const char* crt_directory()
{
  static const char* dir[] =
  {
    "/usr/lib/x86_64-linux-gnu/",
    "/usr/lib64/",
    "/usr/lib/",
    NULL
  };

  for(const char** p = dir; *p != NULL; p++)
  {
    char filename[PATH_MAX];
    strcpy(filename, *p);
    strcat(filename, "crt1.o");

    if(file_exists(filename))
      return *p;
  }

  return NULL;
}

static const char* gccs_directory()
{
  static const char* dir[] =
  {
    "/lib/x86_64-linux-gnu/",
    "/lib64/",
    "/lib/",
    NULL
  };

  for(const char** p = dir; *p != NULL; p++)
  {
    char filename[PATH_MAX];
    strcpy(filename, *p);
    strcat(filename, "libgcc_s.so.1");

    if(file_exists(filename))
      return *p;
  }

  return NULL;
}
#endif

static bool need_primitive_call(compile_t* c, const char* method)
{
  size_t i = HASHMAP_BEGIN;
  reachable_type_t* t;

  while((t = reachable_types_next(c->reachable, &i)) != NULL)
  {
    if(ast_id(t->type) == TK_TUPLETYPE)
      continue;

    ast_t* def = (ast_t*)ast_data(t->type);

    if(ast_id(def) != TK_PRIMITIVE)
      continue;

    reachable_method_name_t* n = reach_method_name(t, method);

    if(n == NULL)
      continue;

    return true;
  }

  return false;
}

static void primitive_call(compile_t* c, const char* method, LLVMValueRef arg)
{
  size_t count = 1;

  if(arg != NULL)
    count++;

  size_t i = HASHMAP_BEGIN;
  reachable_type_t* t;

  while((t = reachable_types_next(c->reachable, &i)) != NULL)
  {
    if(ast_id(t->type) == TK_TUPLETYPE)
      continue;

    ast_t* def = (ast_t*)ast_data(t->type);

    if(ast_id(def) != TK_PRIMITIVE)
      continue;

    reachable_method_name_t* n = reach_method_name(t, method);

    if(n == NULL)
      continue;

    gentype_t g;

    if(!gentype(c, t->type, &g))
    {
      assert(0);
      return;
    }

    LLVMValueRef fun = genfun_proto(c, &g, method, NULL);
    assert(fun != NULL);

    LLVMValueRef args[2];
    args[0] = g.instance;
    args[1] = arg;

    codegen_call(c, fun, args, count);
  }
}

static LLVMValueRef create_main(compile_t* c, gentype_t* g, LLVMValueRef ctx)
{
  // Create the main actor and become it.
  LLVMValueRef args[2];
  args[0] = ctx;
  args[1] = LLVMConstBitCast(g->desc, c->descriptor_ptr);
  LLVMValueRef actor = gencall_runtime(c, "pony_create", args, 2, "");

  args[0] = ctx;
  args[1] = actor;
  gencall_runtime(c, "pony_become", args, 2, "");

  return actor;
}

static void gen_main(compile_t* c, gentype_t* main_g, gentype_t* env_g)
{
  LLVMTypeRef params[3];
  params[0] = c->i32;
  params[1] = LLVMPointerType(LLVMPointerType(c->i8, 0), 0);
  params[2] = LLVMPointerType(LLVMPointerType(c->i8, 0), 0);

  LLVMTypeRef ftype = LLVMFunctionType(c->i32, params, 3, false);
  LLVMValueRef func = LLVMAddFunction(c->module, "main", ftype);

  codegen_startfun(c, func, false);

  LLVMValueRef args[4];
  args[0] = LLVMGetParam(func, 0);
  LLVMSetValueName(args[0], "argc");

  args[1] = LLVMGetParam(func, 1);
  LLVMSetValueName(args[1], "argv");

  args[2] = LLVMGetParam(func, 2);
  LLVMSetValueName(args[1], "envp");

  // Initialise the pony runtime with argc and argv, getting a new argc.
  args[0] = gencall_runtime(c, "pony_init", args, 2, "argc");

  // Create the main actor and become it.
  LLVMValueRef ctx = gencall_runtime(c, "pony_ctx", NULL, 0, "");
  codegen_setctx(c, ctx);
  LLVMValueRef main_actor = create_main(c, main_g, ctx);

  // Create an Env on the main actor's heap.
  const char* env_name = "Env";
  const char* env_create = genname_fun(env_name, "_create", NULL);

  LLVMValueRef env_args[4];
  env_args[0] = gencall_alloc(c, env_g);
  env_args[1] = LLVMBuildZExt(c->builder, args[0], c->i64, "");
  env_args[2] = args[1];
  env_args[3] = args[2];

  LLVMValueRef env = gencall_runtime(c, env_create, env_args, 4, "env");
  LLVMSetInstructionCallConv(env, GEN_CALLCONV);

  // Run primitive initialisers using the main actor's heap.
  primitive_call(c, stringtab("_init"), env);

  // Create a type for the message.
  LLVMTypeRef f_params[4];
  f_params[0] = c->i32;
  f_params[1] = c->i32;
  f_params[2] = c->void_ptr;
  f_params[3] = LLVMTypeOf(env);
  LLVMTypeRef msg_type = LLVMStructTypeInContext(c->context, f_params, 4,
    false);
  LLVMTypeRef msg_type_ptr = LLVMPointerType(msg_type, 0);

  // Allocate the message, setting its size and ID.
  uint32_t index = genfun_vtable_index(c, main_g, stringtab("create"), NULL);

  size_t msg_size = LLVMABISizeOfType(c->target_data, msg_type);
  args[0] = LLVMConstInt(c->i32, pool_index(msg_size), false);
  args[1] = LLVMConstInt(c->i32, index, false);
  LLVMValueRef msg = gencall_runtime(c, "pony_alloc_msg", args, 2, "");
  LLVMValueRef msg_ptr = LLVMBuildBitCast(c->builder, msg, msg_type_ptr, "");

  // Set the message contents.
  LLVMValueRef env_ptr = LLVMBuildStructGEP(c->builder, msg_ptr, 3, "");
  LLVMBuildStore(c->builder, env, env_ptr);

  // Trace the message.
  args[0] = ctx;
  gencall_runtime(c, "pony_gc_send", args, 1, "");

  const char* env_trace = genname_trace(env_name);
  args[0] = ctx;
  args[1] = LLVMBuildBitCast(c->builder, env, c->object_ptr, "");
  args[2] = LLVMGetNamedFunction(c->module, env_trace);
  gencall_runtime(c, "pony_traceobject", args, 3, "");

  args[0] = ctx;
  gencall_runtime(c, "pony_send_done", args, 1, "");

  // Send the message.
  args[0] = ctx;
  args[1] = main_actor;
  args[2] = msg;
  gencall_runtime(c, "pony_sendv", args, 3, "");

  // Start the runtime.
  LLVMValueRef zero = LLVMConstInt(c->i32, 0, false);
  LLVMValueRef rc = gencall_runtime(c, "pony_start", &zero, 1, "");

  // Run primitive finalisers. We create a new main actor as a context to run
  // the finalisers in, but we do not initialise or schedule it.
  if(need_primitive_call(c, stringtab("_final")))
  {
    LLVMValueRef final_actor = create_main(c, main_g, ctx);
    primitive_call(c, stringtab("_final"), NULL);
    args[0] = final_actor;
    gencall_runtime(c, "pony_destroy", args, 1, "");
  }

  // Return the runtime exit code.
  LLVMBuildRet(c->builder, rc);

  codegen_finishfun(c);

  // External linkage for main().
  LLVMSetLinkage(func, LLVMExternalLinkage);
}

static bool link_exe(compile_t* c, ast_t* program,
  const char* file_o)
{
#if defined(PLATFORM_IS_MACOSX)
  char* arch = strchr(c->opt->triple, '-');

  if(arch == NULL)
  {
    errorf(NULL, "couldn't determine architecture from %s", c->opt->triple);
    return false;
  }

  const char* file_exe = suffix_filename(c->opt->output, "", c->filename, "");
  printf("Linking %s\n", file_exe);

  program_lib_build_args(program, "-L", "", "", "-l", "");
  const char* lib_args = program_lib_args(program);

  size_t arch_len = arch - c->opt->triple;
  size_t ld_len = 128 + arch_len + strlen(file_exe) + strlen(file_o) +
    strlen(lib_args);
  char* ld_cmd = (char*)pool_alloc_size(ld_len);

  snprintf(ld_cmd, ld_len,
    "ld -execute -no_pie -dead_strip -arch %.*s -macosx_version_min 10.8 "
    "-o %s %s %s -lponyrt -lSystem",
    (int)arch_len, c->opt->triple, file_exe, file_o, lib_args
    );

  if(system(ld_cmd) != 0)
  {
    errorf(NULL, "unable to link");
    pool_free_size(ld_len, ld_cmd);
    return false;
  }

  pool_free_size(ld_len, ld_cmd);

  if(!c->opt->strip_debug)
  {
    size_t dsym_len = 16 + strlen(file_exe);
    char* dsym_cmd = (char*)pool_alloc_size(dsym_len);

    snprintf(dsym_cmd, dsym_len, "rm -rf %s.dSYM", file_exe);
    system(dsym_cmd);

    snprintf(dsym_cmd, dsym_len, "dsymutil %s", file_exe);

    if(system(dsym_cmd) != 0)
      errorf(NULL, "unable to create dsym");

    pool_free_size(dsym_len, dsym_cmd);
  }

#elif defined(PLATFORM_IS_LINUX) || defined(PLATFORM_IS_FREEBSD)
  const char* file_exe = suffix_filename(c->opt->output, "", c->filename, "");
  printf("Linking %s\n", file_exe);

#ifdef PLATFORM_IS_FREEBSD
  use_path(program, "/usr/local/lib", NULL, NULL);
#endif

  program_lib_build_args(program, "-L", "-Wl,--start-group ", "-Wl,--end-group ",
    "-l", "");
  const char* lib_args = program_lib_args(program);
  const char* crt_dir = crt_directory();
  const char* gccs_dir = gccs_directory();

  if((crt_dir == NULL) || (gccs_dir == NULL))
  {
    errorf(NULL, "could not find CRT");
    return false;
  }

  size_t ld_len = 256 + strlen(file_exe) + strlen(file_o) + strlen(lib_args) +
    strlen(gccs_dir) + (3 * strlen(crt_dir));
  char* ld_cmd = (char*)pool_alloc_size(ld_len);

#if 0
  snprintf(ld_cmd, ld_len,
    "ld --eh-frame-hdr --hash-style=gnu "
#if defined(PLATFORM_IS_LINUX)
    "-m elf_x86_64 -dynamic-linker /lib64/ld-linux-x86-64.so.2 "
#elif defined(PLATFORM_IS_FREEBSD)
    "-m elf_x86_64_fbsd "
#endif
    "-o %s %scrt1.o %scrti.o %s %s -lponyrt -lpthread "
#ifdef PLATFORM_IS_LINUX
    "-ldl "
#endif
    "-lm -lc %slibgcc_s.so.1 %scrtn.o",
    file_exe, crt_dir, crt_dir, file_o, lib_args, gccs_dir, crt_dir
    );
#endif

  snprintf(ld_cmd, ld_len,
    PONY_COMPILER " -o %s -O3 -march=" PONY_ARCH " -mcx16 -flto -fuse-linker-plugin "
#ifdef PLATFORM_IS_LINUX
    "-fuse-ld=gold "
#endif
    "%s %s -lponyrt -lpthread "
#ifdef PLATFORM_IS_LINUX
    "-ldl "
#endif
    "-lm",
    file_exe, file_o, lib_args
    );

  if(system(ld_cmd) != 0)
  {
    errorf(NULL, "unable to link");
    pool_free_size(ld_len, ld_cmd);
    return false;
  }

  pool_free_size(ld_len, ld_cmd);
#elif defined(PLATFORM_IS_WINDOWS)
  vcvars_t vcvars;

  if(!vcvars_get(&vcvars))
  {
    errorf(NULL, "unable to link");
    return false;
  }

  const char* file_exe = suffix_filename(c->opt->output, "", c->filename,
    ".exe");
  printf("Linking %s\n", file_exe);

  program_lib_build_args(program, "/LIBPATH:", "", "", "", ".lib");
  const char* lib_args = program_lib_args(program);

  size_t ld_len = 256 + strlen(file_exe) + strlen(file_o) +
    strlen(vcvars.kernel32) + strlen(vcvars.msvcrt) + strlen(lib_args);
  char* ld_cmd = (char*)pool_alloc_size(ld_len);

  snprintf(ld_cmd, ld_len,
    "cmd /C \"\"%s\" /DEBUG /NOLOGO /MACHINE:X64 "
    "/OUT:%s "
    "%s "
    "/LIBPATH:\"%s\" "
    "/LIBPATH:\"%s\" "
    "%s ponyrt.lib kernel32.lib msvcrt.lib Ws2_32.lib \"",
    vcvars.link, file_exe, file_o, vcvars.kernel32, vcvars.msvcrt, lib_args
    );

  if(system(ld_cmd) == -1)
  {
    errorf(NULL, "unable to link");
    pool_free_size(ld_len, ld_cmd);
    return false;
  }

  pool_free_size(ld_len, ld_cmd);
#endif

  return true;
}

bool genexe(compile_t* c, ast_t* program)
{
  // The first package is the main package. It has to have a Main actor.
  const char* main_actor = stringtab("Main");
  const char* env_class = stringtab("Env");

  ast_t* package = ast_child(program);
  ast_t* main_def = ast_get(package, main_actor, NULL);

  if(main_def == NULL)
  {
    errorf(NULL, "no Main actor found in package '%s'", c->filename);
    return false;
  }

  // Generate the Main actor and the Env class.
  ast_t* main_ast = type_builtin(c->opt, main_def, main_actor);
  ast_t* env_ast = type_builtin(c->opt, main_def, env_class);

  genprim_reachable_init(c, program);
  reach(c->reachable, main_ast, stringtab("create"), NULL);
  reach(c->reachable, env_ast, stringtab("_create"), NULL);
  paint(c->reachable);

  gentype_t main_g;
  gentype_t env_g;

  bool ok = gentype(c, main_ast, &main_g) && gentype(c, env_ast, &env_g);

  if(ok)
    gen_main(c, &main_g, &env_g);

  ast_free_unattached(main_ast);
  ast_free_unattached(env_ast);

  if(!ok)
    return false;

  if(!genopt(c))
    return false;

  const char* file_o = genobj(c);

  if(file_o == NULL)
    return false;

  if(c->opt->limit < PASS_ALL)
    return true;

  if(!link_exe(c, program, file_o))
    return false;

#ifdef PLATFORM_IS_WINDOWS
  _unlink(file_o);
#else
  unlink(file_o);
#endif

  return true;
}
