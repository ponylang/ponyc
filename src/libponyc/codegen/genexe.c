#include "genexe.h"
#include "genopt.h"
#include "genobj.h"
#include "gencall.h"
#include "genname.h"
#include "genprim.h"
#include "../reach/paint.h"
#include "../pkg/package.h"
#include "../pkg/program.h"
#include "../type/assemble.h"
#include "../type/lookup.h"
#include "../../libponyrt/mem/pool.h"
#include <string.h>
#include <assert.h>

#ifdef PLATFORM_IS_POSIX_BASED
#  include <unistd.h>
#endif

static bool need_primitive_call(compile_t* c, const char* method)
{
  size_t i = HASHMAP_BEGIN;
  reachable_type_t* t;

  while((t = reachable_types_next(c->reachable, &i)) != NULL)
  {
    if(t->underlying != TK_PRIMITIVE)
      continue;

    reachable_method_name_t* n = reach_method_name(t, method);

    if(n == NULL)
      continue;

    return true;
  }

  return false;
}

static void primitive_call(compile_t* c, const char* method)
{
  size_t i = HASHMAP_BEGIN;
  reachable_type_t* t;

  while((t = reachable_types_next(c->reachable, &i)) != NULL)
  {
    if(t->underlying != TK_PRIMITIVE)
      continue;

    reachable_method_t* m = reach_method(t, method, NULL);

    if(m == NULL)
      continue;

    codegen_call(c, m->func, &t->instance, 1);
  }
}

static LLVMValueRef create_main(compile_t* c, reachable_type_t* t,
  LLVMValueRef ctx)
{
  // Create the main actor and become it.
  LLVMValueRef args[2];
  args[0] = ctx;
  args[1] = LLVMConstBitCast(t->desc, c->descriptor_ptr);
  LLVMValueRef actor = gencall_runtime(c, "pony_create", args, 2, "");

  args[0] = ctx;
  args[1] = actor;
  gencall_runtime(c, "pony_become", args, 2, "");

  return actor;
}

static void gen_main(compile_t* c, reachable_type_t* t_main,
  reachable_type_t* t_env)
{
  LLVMTypeRef params[3];
  params[0] = c->i32;
  params[1] = LLVMPointerType(LLVMPointerType(c->i8, 0), 0);
  params[2] = LLVMPointerType(LLVMPointerType(c->i8, 0), 0);

  LLVMTypeRef ftype = LLVMFunctionType(c->i32, params, 3, false);
  LLVMValueRef func = LLVMAddFunction(c->module, "main", ftype);

  codegen_startfun(c, func, NULL, NULL);

  LLVMValueRef args[4];
  args[0] = LLVMGetParam(func, 0);
  LLVMSetValueName(args[0], "argc");

  args[1] = LLVMGetParam(func, 1);
  LLVMSetValueName(args[1], "argv");

  args[2] = LLVMGetParam(func, 2);
  LLVMSetValueName(args[2], "envp");

  // Initialise the pony runtime with argc and argv, getting a new argc.
  args[0] = gencall_runtime(c, "pony_init", args, 2, "argc");

  // Create the main actor and become it.
  LLVMValueRef ctx = gencall_runtime(c, "pony_ctx", NULL, 0, "");
  codegen_setctx(c, ctx);
  LLVMValueRef main_actor = create_main(c, t_main, ctx);

  // Create an Env on the main actor's heap.
  const char* env_name = "Env";
  const char* env_create = genname_fun(env_name, "_create", NULL);

  LLVMValueRef env_args[4];
  env_args[0] = gencall_alloc(c, t_env);
  env_args[1] = args[0];
  env_args[2] = LLVMBuildBitCast(c->builder, args[1], c->void_ptr, "");
  env_args[3] = LLVMBuildBitCast(c->builder, args[2], c->void_ptr, "");

  LLVMValueRef env = gencall_runtime(c, env_create, env_args, 4, "env");
  LLVMSetInstructionCallConv(env, c->callconv);

  // Run primitive initialisers using the main actor's heap.
  primitive_call(c, c->str__init);

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
  uint32_t index = reach_vtable_index(t_main, "create");
  size_t msg_size = (size_t)LLVMABISizeOfType(c->target_data, msg_type);
  args[0] = LLVMConstInt(c->i32, ponyint_pool_index(msg_size), false);
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
  args[3] = LLVMConstInt(c->i32, 1, false);
  gencall_runtime(c, "pony_traceobject", args, 4, "");

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
  if(need_primitive_call(c, c->str__final))
  {
    LLVMValueRef final_actor = create_main(c, t_main, ctx);
    primitive_call(c, c->str__final);
    args[0] = final_actor;
    gencall_runtime(c, "ponyint_destroy", args, 1, "");
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
  errors_t* errors = c->opt->check.errors;

#if defined(PLATFORM_IS_MACOSX)
  char* arch = strchr(c->opt->triple, '-');

  if(arch == NULL)
  {
    errorf(errors, NULL, "couldn't determine architecture from %s",
      c->opt->triple);
    return false;
  }

  const char* file_exe =
    suffix_filename(c, c->opt->output, "", c->filename, "");

  PONY_LOG(c->opt, VERBOSITY_MINIMAL, ("Linking %s\n", file_exe));

  program_lib_build_args(program, c->opt, "-L", NULL, "", "", "-l", "");
  const char* lib_args = program_lib_args(program);

  size_t arch_len = arch - c->opt->triple;
  size_t ld_len = 128 + arch_len + strlen(file_exe) + strlen(file_o) +
    strlen(lib_args);
  char* ld_cmd = (char*)ponyint_pool_alloc_size(ld_len);

  // Avoid incorrect ld, eg from macports.
  snprintf(ld_cmd, ld_len,
    "/usr/bin/ld -execute -no_pie -dead_strip -arch %.*s "
    "-macosx_version_min 10.8 -o %s %s %s -lponyrt -lSystem",
    (int)arch_len, c->opt->triple, file_exe, file_o, lib_args
    );

  PONY_LOG(c->opt, VERBOSITY_TOOL_INFO, ("%s\n", ld_cmd));

  if(system(ld_cmd) != 0)
  {
    errorf(errors, NULL, "unable to link: %s", ld_cmd);
    ponyint_pool_free_size(ld_len, ld_cmd);
    return false;
  }

  ponyint_pool_free_size(ld_len, ld_cmd);

  if(!c->opt->strip_debug)
  {
    size_t dsym_len = 16 + strlen(file_exe);
    char* dsym_cmd = (char*)ponyint_pool_alloc_size(dsym_len);

    snprintf(dsym_cmd, dsym_len, "rm -rf %s.dSYM", file_exe);
    system(dsym_cmd);

    snprintf(dsym_cmd, dsym_len, "dsymutil %s", file_exe);

    if(system(dsym_cmd) != 0)
      errorf(errors, NULL, "unable to create dsym");

    ponyint_pool_free_size(dsym_len, dsym_cmd);
  }

#elif defined(PLATFORM_IS_LINUX) || defined(PLATFORM_IS_FREEBSD)
  const char* file_exe =
    suffix_filename(c, c->opt->output, "", c->filename, "");

  PONY_LOG(c->opt, VERBOSITY_MINIMAL, ("Linking %s\n", file_exe));

  program_lib_build_args(program, c->opt, "-L", "-Wl,-rpath,",
    "-Wl,--start-group ", "-Wl,--end-group ", "-l", "");
  const char* lib_args = program_lib_args(program);

  size_t ld_len = 512 + strlen(file_exe) + strlen(file_o) + strlen(lib_args);
  char* ld_cmd = (char*)ponyint_pool_alloc_size(ld_len);

  snprintf(ld_cmd, ld_len, PONY_COMPILER " -o %s -O3 -march=" PONY_ARCH " "
#ifndef PLATFORM_IS_ILP32
    "-mcx16 "
#endif

#ifdef PONY_USE_LTO
    "-flto -fuse-linker-plugin "
#endif

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

  PONY_LOG(c->opt, VERBOSITY_TOOL_INFO, ("%s\n", ld_cmd));

  if(system(ld_cmd) != 0)
  {
    errorf(errors, NULL, "unable to link: %s", ld_cmd);
    ponyint_pool_free_size(ld_len, ld_cmd);
    return false;
  }

  ponyint_pool_free_size(ld_len, ld_cmd);
#elif defined(PLATFORM_IS_WINDOWS)
  vcvars_t vcvars;

  if(!vcvars_get(&vcvars, errors))
  {
    errorf(errors, NULL, "unable to link: no vcvars");
    return false;
  }

  const char* file_exe = suffix_filename(c, c->opt->output, "", c->filename,
    ".exe");
  PONY_LOG(c->opt, VERBOSITY_MINIMAL, ("Linking %s\n", file_exe));

  program_lib_build_args(program, c->opt,
    "/LIBPATH:", NULL, "", "", "", ".lib");
  const char* lib_args = program_lib_args(program);

  size_t ld_len = 256 + strlen(file_exe) + strlen(file_o) +
    strlen(vcvars.kernel32) + strlen(vcvars.msvcrt) + strlen(lib_args);
  char* ld_cmd = (char*)ponyint_pool_alloc_size(ld_len);

  while (true)
  {
    int num_written = snprintf(ld_cmd, ld_len,
      "cmd /C \"\"%s\" /DEBUG /NOLOGO /MACHINE:X64 "
      "/OUT:%s "
      "%s "
      "/LIBPATH:\"%s\" "
      "/LIBPATH:\"%s\" "
      "%s kernel32.lib msvcrt.lib Ws2_32.lib vcruntime.lib legacy_stdio_definitions.lib ponyrt.lib \"",
      vcvars.link, file_exe, file_o, vcvars.kernel32, vcvars.msvcrt, lib_args
    );

    if (num_written < ld_len)
      break;

    ponyint_pool_free_size(ld_len, ld_cmd);
    ld_len += 256;
    ld_cmd = (char*)ponyint_pool_alloc_size(ld_len);
  }

  PONY_LOG(c->opt, VERBOSITY_TOOL_INFO, ("%s\n", ld_cmd));

  if (system(ld_cmd) == -1)
  {
    errorf(errors, NULL, "unable to link: %s", ld_cmd);
    ponyint_pool_free_size(ld_len, ld_cmd);
    return false;
  }

  ponyint_pool_free_size(ld_len, ld_cmd);
#endif

  return true;
}

bool genexe(compile_t* c, ast_t* program)
{
  errors_t* errors = c->opt->check.errors;

  // The first package is the main package. It has to have a Main actor.
  const char* main_actor = c->str_Main;
  const char* env_class = c->str_Env;

  ast_t* package = ast_child(program);
  ast_t* main_def = ast_get(package, main_actor, NULL);

  if(main_def == NULL)
  {
    errorf(errors, NULL, "no Main actor found in package '%s'", c->filename);
    return false;
  }

  // Generate the Main actor and the Env class.
  ast_t* main_ast = type_builtin(c->opt, main_def, main_actor);
  ast_t* env_ast = type_builtin(c->opt, main_def, env_class);

  if(lookup(NULL, main_ast, main_ast, c->str_create) == NULL)
    return false;

  PONY_LOG(c->opt, VERBOSITY_INFO, (" Reachability\n"));
  reach(c->reachable, &c->next_type_id, main_ast, c->str_create, NULL, c->opt);
  reach(c->reachable, &c->next_type_id, env_ast, c->str__create, NULL, c->opt);

  PONY_LOG(c->opt, VERBOSITY_INFO, (" Selector painting\n"));
  paint(c->reachable);

  if(c->opt->verbosity >= VERBOSITY_ALL)
    reach_dump(c->reachable);

  if(!gentypes(c))
    return false;

  reachable_type_t* t_main = reach_type(c->reachable, main_ast);
  reachable_type_t* t_env = reach_type(c->reachable, env_ast);

  if((t_main == NULL) || (t_env == NULL))
    return false;

  gen_main(c, t_main, t_env);

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
