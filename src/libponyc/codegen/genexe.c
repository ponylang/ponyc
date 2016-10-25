#include "genexe.h"
#include "genopt.h"
#include "genobj.h"
#include "gencall.h"
#include "genname.h"
#include "genprim.h"
#include "genopt.h"
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
  reach_type_t* t;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    if(t->underlying != TK_PRIMITIVE)
      continue;

    reach_method_name_t* n = reach_method_name(t, method);

    if(n == NULL)
      continue;

    return true;
  }

  return false;
}

static void primitive_call(compile_t* c, const char* method)
{
  size_t i = HASHMAP_BEGIN;
  reach_type_t* t;

  while((t = reach_types_next(&c->reach->types, &i)) != NULL)
  {
    if(t->underlying != TK_PRIMITIVE)
      continue;

    reach_method_t* m = reach_method(t, TK_NONE, method, NULL);

    if(m == NULL)
      continue;

    LLVMValueRef value = codegen_call(c, m->func, &t->instance, 1);

    if(c->str__final == method)
      LLVMSetInstructionCallConv(value, LLVMCCallConv);
  }
}

static LLVMValueRef create_main(compile_t* c, reach_type_t* t,
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

static void gen_main(compile_t* c, reach_type_t* t_main,
  reach_type_t* t_env)
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
  reach_method_t* m = reach_method(t_env, TK_NONE, c->str__create, NULL);

  LLVMValueRef env_args[4];
  env_args[0] = gencall_alloc(c, t_env);
  env_args[1] = args[0];
  env_args[2] = LLVMBuildBitCast(c->builder, args[1], c->void_ptr, "");
  env_args[3] = LLVMBuildBitCast(c->builder, args[2], c->void_ptr, "");
  codegen_call(c, m->func, env_args, 4);
  LLVMValueRef env = env_args[0];

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
  uint32_t index = reach_vtable_index(t_main, c->str_create);
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

  args[0] = ctx;
  args[1] = LLVMBuildBitCast(c->builder, env, c->object_ptr, "");
  args[2] = LLVMBuildBitCast(c->builder, t_env->desc, c->descriptor_ptr, "");
  args[3] = LLVMConstInt(c->i32, PONY_TRACE_IMMUTABLE, false);
  gencall_runtime(c, "pony_traceknown", args, 4, "");

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

#if defined(PLATFORM_IS_LINUX) || defined(PLATFORM_IS_FREEBSD)
static const char* env_cc_or_pony_compiler(bool* out_fallback_linker)
{
  const char* cc = getenv("CC");
  if(cc == NULL)
  {
    *out_fallback_linker = true;
    return PONY_COMPILER;
  }
  return cc;
}
#endif

static bool link_exe(compile_t* c, ast_t* program,
  const char* file_o)
{
  errors_t* errors = c->opt->check.errors;

  const char* ponyrt = c->opt->runtimebc ? "" :
#if defined(PLATFORM_IS_WINDOWS)
    "ponyrt.lib";
#elif defined(PLATFORM_IS_LINUX)
    c->opt->pic ? "-lponyrt-pic" : "-lponyrt";
#else
    "-lponyrt";
#endif

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

  if(c->opt->verbosity >= VERBOSITY_MINIMAL)
    fprintf(stderr, "Linking %s\n", file_exe);

  program_lib_build_args(program, c->opt, "-L", NULL, "", "", "-l", "");
  const char* lib_args = program_lib_args(program);

  size_t arch_len = arch - c->opt->triple;
  size_t ld_len = 128 + arch_len + strlen(file_exe) + strlen(file_o) +
    strlen(lib_args);
  char* ld_cmd = (char*)ponyint_pool_alloc_size(ld_len);
  const char* linker = c->opt->linker != NULL ? c->opt->linker
                                              : "ld";

  snprintf(ld_cmd, ld_len,
    "%s -execute -no_pie -dead_strip -arch %.*s "
    "-macosx_version_min 10.8 -o %s %s %s %s -lSystem",
           linker, (int)arch_len, c->opt->triple, file_exe, file_o,
           lib_args, ponyrt
    );

  if(c->opt->verbosity >= VERBOSITY_TOOL_INFO)
    fprintf(stderr, "%s\n", ld_cmd);

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

  if(c->opt->verbosity >= VERBOSITY_MINIMAL)
    fprintf(stderr, "Linking %s\n", file_exe);

  program_lib_build_args(program, c->opt, "-L", "-Wl,-rpath,",
    "-Wl,--start-group ", "-Wl,--end-group ", "-l", "");
  const char* lib_args = program_lib_args(program);

  const char* arch = c->opt->link_arch != NULL ? c->opt->link_arch : PONY_ARCH;
  bool fallback_linker = false;
  const char* linker = c->opt->linker != NULL ? c->opt->linker :
    env_cc_or_pony_compiler(&fallback_linker);

  if((c->opt->verbosity >= VERBOSITY_MINIMAL) && fallback_linker)
  {
    fprintf(stderr,
      "Warning: environment variable $CC undefined, using %s as the linker\n",
      PONY_COMPILER);
  }
  const char* mcx16_arg = target_is_ilp32(c->opt->triple) ? "" : "-mcx16";
  const char* fuseld = target_is_linux(c->opt->triple) ? "-fuse-ld=gold " : "";
  const char* ldl = target_is_linux(c->opt->triple) ? "-ldl  " : "";

  size_t ld_len = 512 + strlen(file_exe) + strlen(file_o) + strlen(lib_args)
                  + strlen(arch) + strlen(mcx16_arg) + strlen(fuseld)
                  + strlen(ldl);

  char* ld_cmd = (char*)ponyint_pool_alloc_size(ld_len);

  snprintf(ld_cmd, ld_len, "%s -o %s -O3 -march=%s "
    "%s "
#ifdef PONY_USE_LTO
    "-flto -fuse-linker-plugin "
#endif
    "%s %s %s %s -lpthread %s "
    "-lm",
    linker, file_exe, arch, mcx16_arg, fuseld, file_o, lib_args, ponyrt, ldl
    );

  if(c->opt->verbosity >= VERBOSITY_TOOL_INFO)
    fprintf(stderr, "%s\n", ld_cmd);

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
  if(c->opt->verbosity >= VERBOSITY_MINIMAL)
    fprintf(stderr, "Linking %s\n", file_exe);

  program_lib_build_args(program, c->opt,
    "/LIBPATH:", NULL, "", "", "", ".lib");
  const char* lib_args = program_lib_args(program);

  char ucrt_lib[MAX_PATH + 12];
  if (strlen(vcvars.ucrt) > 0)
    snprintf(ucrt_lib, MAX_PATH + 12, "/LIBPATH:\"%s\"", vcvars.ucrt);
  else
    ucrt_lib[0] = '\0';

  size_t ld_len = 256 + strlen(file_exe) + strlen(file_o) +
    strlen(vcvars.kernel32) + strlen(vcvars.msvcrt) + strlen(lib_args);
  char* ld_cmd = (char*)ponyint_pool_alloc_size(ld_len);

  while (true)
  {
    int num_written = snprintf(ld_cmd, ld_len,
      "cmd /C \"\"%s\" /DEBUG /NOLOGO /MACHINE:X64 "
      "/OUT:%s "
      "%s %s "
      "/LIBPATH:\"%s\" "
      "/LIBPATH:\"%s\" "
      "%s %s %s \"",
      vcvars.link, file_exe, file_o, ucrt_lib, vcvars.kernel32, 
      vcvars.msvcrt, lib_args, vcvars.default_libs, ponyrt
    );

    if (num_written < ld_len)
      break;

    ponyint_pool_free_size(ld_len, ld_cmd);
    ld_len += 256;
    ld_cmd = (char*)ponyint_pool_alloc_size(ld_len);
  }

  if(c->opt->verbosity >= VERBOSITY_TOOL_INFO)
    fprintf(stderr, "%s\n", ld_cmd);

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

  if(lookup(c->opt, main_ast, main_ast, c->str_create) == NULL)
    return false;

  if(c->opt->verbosity >= VERBOSITY_INFO)
    fprintf(stderr, " Reachability\n");
  reach(c->reach, main_ast, c->str_create, NULL, c->opt);
  reach(c->reach, env_ast, c->str__create, NULL, c->opt);

  if(c->opt->limit == PASS_REACH)
    return true;

  if(c->opt->verbosity >= VERBOSITY_INFO)
    fprintf(stderr, " Selector painting\n");
  paint(&c->reach->types);

  if(c->opt->limit == PASS_PAINT)
    return true;

  if(!gentypes(c))
    return false;

  if(c->opt->verbosity >= VERBOSITY_ALL)
    reach_dump(c->reach);

  reach_type_t* t_main = reach_type(c->reach, main_ast);
  reach_type_t* t_env = reach_type(c->reach, env_ast);

  if((t_main == NULL) || (t_env == NULL))
    return false;

  gen_main(c, t_main, t_env);

  if(!genopt(c, true))
    return false;

  if(c->opt->runtimebc)
  {
    if(!codegen_merge_runtime_bitcode(c))
      return false;

    // Rerun the optimiser without the Pony-specific optimisation passes.
    // Inlining runtime functions can screw up these passes so we can't
    // run the optimiser only once after merging.
    if(!genopt(c, false))
      return false;
  }

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
