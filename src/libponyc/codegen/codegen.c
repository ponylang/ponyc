#include "codegen.h"
#include "genprim.h"
#include "genname.h"
#include "gentype.h"
#include "gendesc.h"
#include "genfun.h"
#include "gencall.h"
#include "../debug/dwarf.h"
#include "../pkg/package.h"
#include "../pkg/program.h"
#include "../ast/error.h"
#include "../ast/stringtab.h"
#include "../../libponyrt/mem/pool.h"
#include <platform.h>

#include <llvm-c/Initialization.h>
#include <llvm-c/BitWriter.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/stat.h>

#ifdef PLATFORM_IS_POSIX_BASED
#  include <unistd.h>
#else
   //disable warnings of unlink being deprecated
#  pragma warning(disable:4996)
#endif

static LLVMPassManagerBuilderRef pmb;
static LLVMPassManagerRef mpm;
static LLVMPassManagerRef lpm;

static void codegen_fatal(const char* reason)
{
  printf("%s\n", reason);
  print_errors();
}

static compile_frame_t* push_frame(compile_t* c)
{
  compile_frame_t* frame = POOL_ALLOC(compile_frame_t);
  memset(frame, 0, sizeof(compile_frame_t));

  frame->prev = c->frame;
  c->frame = frame;

  return frame;
}

static void pop_frame(compile_t* c)
{
  compile_frame_t* frame = c->frame;
  c->frame = frame->prev;
  POOL_FREE(compile_frame_t, frame);
}

static LLVMTargetMachineRef make_machine(pass_opt_t* opt)
{
  LLVMTargetRef target;
  char* err;

  if(LLVMGetTargetFromTriple(opt->triple, &target, &err) != 0)
  {
    errorf(NULL, "couldn't create target: %s", err);
    LLVMDisposeMessage(err);
    return NULL;
  }

  LLVMCodeGenOptLevel opt_level =
    opt->release ? LLVMCodeGenLevelAggressive : LLVMCodeGenLevelNone;

  LLVMTargetMachineRef machine = LLVMCreateTargetMachine(target, opt->triple,
    opt->cpu, opt->features, opt_level, LLVMRelocDefault, LLVMCodeModelDefault);

  if(machine == NULL)
  {
    errorf(NULL, "couldn't create target machine");
    return NULL;
  }

  return machine;
}

static void init_runtime(compile_t* c)
{
  c->str_1 = stringtab("$1");
  c->str_Bool = stringtab("Bool");
  c->str_I8 = stringtab("I8");
  c->str_I16 = stringtab("I16");
  c->str_I32 = stringtab("I32");
  c->str_I64 = stringtab("I64");
  c->str_I128 = stringtab("I128");
  c->str_U8 = stringtab("U8");
  c->str_U16 = stringtab("U16");
  c->str_U32 = stringtab("U32");
  c->str_U64 = stringtab("U64");
  c->str_U128 = stringtab("U128");
  c->str_F32 = stringtab("F32");
  c->str_F64 = stringtab("F64");
  c->str_Pointer = stringtab("Pointer");
  c->str_Array = stringtab("Array");
  c->str_Platform = stringtab("Platform");

  c->str_add = stringtab("add");
  c->str_sub = stringtab("sub");
  c->str_mul = stringtab("mul");
  c->str_div = stringtab("div");
  c->str_mod = stringtab("mod");
  c->str_neg = stringtab("neg");
  c->str_and = stringtab("op_and");
  c->str_or = stringtab("op_or");
  c->str_xor = stringtab("op_xor");
  c->str_not = stringtab("op_not");
  c->str_shl = stringtab("shl");
  c->str_shr = stringtab("shr");
  c->str_eq = stringtab("eq");
  c->str_ne = stringtab("ne");
  c->str_lt = stringtab("lt");
  c->str_le = stringtab("le");
  c->str_ge = stringtab("ge");
  c->str_gt = stringtab("gt");

  LLVMTypeRef type;
  LLVMTypeRef params[4];

  c->void_type = LLVMVoidTypeInContext(c->context);
  c->i1 = LLVMInt1TypeInContext(c->context);
  c->i8 = LLVMInt8TypeInContext(c->context);
  c->i16 = LLVMInt16TypeInContext(c->context);
  c->i32 = LLVMInt32TypeInContext(c->context);
  c->i64 = LLVMInt64TypeInContext(c->context);
  c->i128 = LLVMIntTypeInContext(c->context, 128);
  c->f32 = LLVMFloatTypeInContext(c->context);
  c->f64 = LLVMDoubleTypeInContext(c->context);
  c->intptr = LLVMIntPtrTypeInContext(c->context, c->target_data);

  // i8*
  c->void_ptr = LLVMPointerType(c->i8, 0);

  // forward declare object
  c->object_type = LLVMStructCreateNamed(c->context, "$object");
  c->object_ptr = LLVMPointerType(c->object_type, 0);

  // padding required in an actor between the descriptor and fields
  c->actor_pad = LLVMArrayType(c->i8, 265);

  // message
  params[0] = c->i32; // size
  params[1] = c->i32; // id
  c->msg_type = LLVMStructCreateNamed(c->context, "$message");
  c->msg_ptr = LLVMPointerType(c->msg_type, 0);
  LLVMStructSetBody(c->msg_type, params, 2, false);

  // trace
  // void (*)($object*)
  params[0] = c->object_ptr;
  c->trace_type = LLVMFunctionType(c->void_type, params, 1, false);
  c->trace_fn = LLVMPointerType(c->trace_type, 0);

  // dispatch
  // void (*)($object*, $message*)
  params[0] = c->object_ptr;
  params[1] = c->msg_ptr;
  c->dispatch_type = LLVMFunctionType(c->void_type, params, 2, false);
  c->dispatch_fn = LLVMPointerType(c->dispatch_type, 0);

  // void (*)($object*)
  params[0] = c->object_ptr;
  c->final_fn = LLVMPointerType(
    LLVMFunctionType(c->void_type, params, 1, false), 0);

  // descriptor, opaque version
  // We need this in order to build our own structure.
  const char* desc_name = genname_descriptor(NULL);
  c->descriptor_type = LLVMStructCreateNamed(c->context, desc_name);
  c->descriptor_ptr = LLVMPointerType(c->descriptor_type, 0);

  // field descriptor
  // Also needed to build a descriptor structure.
  params[0] = c->i32;
  params[1] = c->descriptor_ptr;
  c->field_descriptor = LLVMStructTypeInContext(c->context, params, 2, false);

  // descriptor, filled in
  c->descriptor_type = gendesc_type(c, NULL);

  // define object
  params[0] = c->descriptor_ptr;
  LLVMStructSetBody(c->object_type, params, 1, false);

  // $object* pony_create($desc*)
  params[0] = c->descriptor_ptr;
  type = LLVMFunctionType(c->object_ptr, params, 1, false);
  LLVMAddFunction(c->module, "pony_create", type);

  // void pony_sendv($object*, $message*);
  params[0] = c->object_ptr;
  params[1] = c->msg_ptr;
  type = LLVMFunctionType(c->void_type, params, 2, false);
  LLVMAddFunction(c->module, "pony_sendv", type);

  // i8* pony_alloc(i64)
  params[0] = c->i64;
  type = LLVMFunctionType(c->void_ptr, params, 1, false);
  LLVMAddFunction(c->module, "pony_alloc", type);

  // i8* pony_realloc(i8*, i64)
  params[0] = c->void_ptr;
  params[1] = c->i64;
  type = LLVMFunctionType(c->void_ptr, params, 2, false);
  LLVMAddFunction(c->module, "pony_realloc", type);

  // $message* pony_alloc_msg(i32, i32)
  params[0] = c->i32;
  params[1] = c->i32;
  type = LLVMFunctionType(c->msg_ptr, params, 2, false);
  LLVMAddFunction(c->module, "pony_alloc_msg", type);

  // void pony_trace(i8*)
  params[0] = c->void_ptr;
  type = LLVMFunctionType(c->void_type, params, 1, false);
  LLVMAddFunction(c->module, "pony_trace", type);

  // void pony_traceactor($object*)
  params[0] = c->object_ptr;
  type = LLVMFunctionType(c->void_type, params, 1, false);
  LLVMAddFunction(c->module, "pony_traceactor", type);

  // void pony_traceobject($object*, trace_fn)
  params[0] = c->object_ptr;
  params[1] = c->trace_fn;
  type = LLVMFunctionType(c->void_type, params, 2, false);
  LLVMAddFunction(c->module, "pony_traceobject", type);

  // void pony_gc_send()
  type = LLVMFunctionType(c->void_type, NULL, 0, false);
  LLVMAddFunction(c->module, "pony_gc_send", type);

  // void pony_gc_recv()
  type = LLVMFunctionType(c->void_type, NULL, 0, false);
  LLVMAddFunction(c->module, "pony_gc_recv", type);

  // void pony_send_done()
  type = LLVMFunctionType(c->void_type, NULL, 0, false);
  LLVMAddFunction(c->module, "pony_send_done", type);

  // void pony_recv_done()
  type = LLVMFunctionType(c->void_type, NULL, 0, false);
  LLVMAddFunction(c->module, "pony_recv_done", type);

  // i32 pony_init(i32, i8**)
  params[0] = c->i32;
  params[1] = LLVMPointerType(c->void_ptr, 0);
  type = LLVMFunctionType(c->i32, params, 2, false);
  LLVMAddFunction(c->module, "pony_init", type);

  // void pony_become($object*)
  params[0] = c->object_ptr;
  type = LLVMFunctionType(c->void_type, params, 1, false);
  LLVMAddFunction(c->module, "pony_become", type);

  // i32 pony_start(i32)
  params[0] = c->i32;
  type = LLVMFunctionType(c->i32, params, 1, false);
  LLVMAddFunction(c->module, "pony_start", type);

  // void pony_throw()
  type = LLVMFunctionType(c->void_type, NULL, 0, false);
  LLVMAddFunction(c->module, "pony_throw", type);

  // i32 pony_personality_v0(...)
  type = LLVMFunctionType(c->i32, NULL, 0, true);
  c->personality = LLVMAddFunction(c->module, "pony_personality_v0", type);

  // i8* memcpy(...)
  type = LLVMFunctionType(c->void_ptr, NULL, 0, true);
  LLVMAddFunction(c->module, "memcpy", type);

  // i8* memmove(...)
  type = LLVMFunctionType(c->void_ptr, NULL, 0, true);
  LLVMAddFunction(c->module, "memmove", type);
}

static int behaviour_index(gentype_t* g, const char* name)
{
  ast_t* def = (ast_t*)ast_data(g->ast);
  ast_t* members = ast_childidx(def, 4);
  ast_t* member = ast_child(members);
  int index = 0;

  while(member != NULL)
  {
    switch(ast_id(member))
    {
      case TK_NEW:
      case TK_BE:
      {
        AST_GET_CHILDREN(member, ignore, id);

        if(ast_name(id) == name)
          return index;

        index++;
        break;
      }

      default: {}
    }

    member = ast_sibling(member);
  }

  return -1;
}

static void codegen_main(compile_t* c, gentype_t* main_g, gentype_t* env_g)
{
  LLVMTypeRef params[2];
  params[0] = c->i32;
  params[1] = LLVMPointerType(LLVMPointerType(c->i8, 0), 0);

  LLVMTypeRef ftype = LLVMFunctionType(c->i32, params, 2, false);
  LLVMValueRef func = LLVMAddFunction(c->module, "main", ftype);

  codegen_startfun(c, func);

  LLVMValueRef args[3];
  args[0] = LLVMGetParam(func, 0);
  LLVMSetValueName(args[0], "argc");

  args[1] = LLVMGetParam(func, 1);
  LLVMSetValueName(args[1], "argv");

  // Initialise the pony runtime with argc and argv, getting a new argc.
  args[0] = gencall_runtime(c, "pony_init", args, 2, "argc");

  // Create the main actor and become it.
  LLVMValueRef m = gencall_create(c, main_g);
  LLVMValueRef object = LLVMBuildBitCast(c->builder, m, c->object_ptr, "");
  gencall_runtime(c, "pony_become", &object, 1, "");

  // Create an Env on the main actor's heap.
  const char* env_name = "Env";
  const char* env_create = genname_fun(env_name, "_create", NULL);
  args[2] = args[1];
  args[1] = LLVMBuildZExt(c->builder, args[0], c->i64, "");
  args[0] = gencall_alloc(c, env_g);
  LLVMValueRef env = gencall_runtime(c, env_create, args, 3, "env");
  LLVMSetInstructionCallConv(env, GEN_CALLCONV);

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
  int index = behaviour_index(main_g, stringtab("create"));
  args[1] = LLVMConstInt(c->i32, 0, false);
  args[0] = LLVMConstInt(c->i32, index, false);
  LLVMValueRef msg = gencall_runtime(c, "pony_alloc_msg", args, 2, "");
  LLVMValueRef msg_ptr = LLVMBuildBitCast(c->builder, msg, msg_type_ptr, "");

  // Set the message contents.
  LLVMValueRef env_ptr = LLVMBuildStructGEP(c->builder, msg_ptr, 3, "");
  LLVMBuildStore(c->builder, env, env_ptr);

  // Trace the message.
  gencall_runtime(c, "pony_gc_send", NULL, 0, "");
  const char* env_trace = genname_trace(env_name);

  args[0] = LLVMBuildBitCast(c->builder, env, c->object_ptr, "");
  args[1] = LLVMGetNamedFunction(c->module, env_trace);
  gencall_runtime(c, "pony_traceobject", args, 2, "");
  gencall_runtime(c, "pony_send_done", NULL, 0, "");

  // Send the message.
  args[0] = object;
  args[1] = msg;
  gencall_runtime(c, "pony_sendv", args, 2, "");

  // Start the runtime.
  LLVMValueRef zero = LLVMConstInt(c->i32, 0, false);
  LLVMValueRef rc = gencall_runtime(c, "pony_start", &zero, 1, "");

  // Return the runtime exit code.
  LLVMBuildRet(c->builder, rc);

  codegen_finishfun(c);

  // External linkage for main().
  LLVMSetLinkage(func, LLVMExternalLinkage);
}

static bool codegen_program(compile_t* c, ast_t* program)
{
  // The first package is the main package. It has to have a Main actor.
  const char* main_actor = stringtab("Main");
  ast_t* package = ast_child(program);
  ast_t* main_def = ast_get(package, main_actor, NULL);

  if(main_def == NULL)
  {
    errorf(NULL, "no Main actor found in package '%s'", c->filename);
    return false;
  }

  // Generate the Main actor and the Env class.
  gentype_t main_g;
  ast_t* main_ast = genprim(c, main_def, main_actor, &main_g);

  if(main_ast == NULL)
    return false;

  const char* env_class = stringtab("Env");

  gentype_t env_g;
  ast_t* env_ast = genprim(c, main_def, env_class, &env_g);

  if(env_ast == NULL)
  {
    ast_free_unattached(main_ast);
    return false;
  }

  codegen_main(c, &main_g, &env_g);

  ast_free_unattached(main_ast);
  ast_free_unattached(env_ast);
  return true;
}

static bool codegen_library(compile_t* c, ast_t* program)
{
  // Look for C-API actors in every package.
  bool found = false;
  ast_t* package = ast_child(program);

  while(package != NULL)
  {
    ast_t* module = ast_child(package);

    while(module != NULL)
    {
      ast_t* entity = ast_child(module);

      while(entity != NULL)
      {
        if(ast_id(entity) == TK_ACTOR)
        {
          ast_t* c_api = ast_childidx(entity, 5);

          if(ast_id(c_api) == TK_AT)
          {
            // We have an actor marked as C-API.
            ast_t* id = ast_child(entity);

            // Generate the actor.
            gentype_t g;
            ast_t* ast = genprim(c, entity, ast_name(id), &g);

            if(ast == NULL)
              return false;

            found = true;
          }
        }

        entity = ast_sibling(entity);
      }

      module = ast_sibling(module);
    }

    package = ast_sibling(package);
  }

  if(!found)
  {
    errorf(NULL, "no C-API actors found in package '%s'", c->filename);
    return false;
  }

  return true;
}

static void init_module(compile_t* c, ast_t* program, pass_opt_t* opt)
{
  c->painter = painter_create();
  painter_colour(c->painter, program);

  // The name of the first package is the name of the program.
  c->filename = package_filename(ast_child(program));

  // Keep track of whether or not we're optimising or emitting debug symbols.
  c->release = opt->release;
  c->library = opt->library;
  c->symbols = opt->symbols;
  c->ieee_math = opt->ieee_math;
  c->no_restrict = opt->no_restrict;

  // LLVM context and machine settings.
  c->context = LLVMContextCreate();
  c->machine = make_machine(opt);
  c->target_data = LLVMGetTargetMachineData(c->machine);

  // Create a module.
  c->module = LLVMModuleCreateWithNameInContext(c->filename, c->context);

  // Set the target triple.
  LLVMSetTarget(c->module, opt->triple);

  // IR builder.
  c->builder = LLVMCreateBuilderInContext(c->context);

  // Function pass manager.
  c->fpm = LLVMCreateFunctionPassManagerForModule(c->module);
  LLVMAddTargetData(c->target_data, c->fpm);
  LLVMPassManagerBuilderPopulateFunctionPassManager(pmb, c->fpm);
  LLVMInitializeFunctionPassManager(c->fpm);

  // Empty frame stack.
  c->frame = NULL;
}

static size_t link_path_length()
{
  strlist_t* p = package_paths();
  size_t len = 0;

  while(p != NULL)
  {
    const char* path = strlist_data(p);
    len += strlen(path);
#ifdef PLATFORM_IS_POSIX_BASED
    len += 6;
#else
    len += 12;
#endif
    p = strlist_next(p);
  }

  return len;
}

static void append_link_paths(char* str)
{
  strlist_t* p = package_paths();

  while(p != NULL)
  {
    const char* path = strlist_data(p);
#ifdef PLATFORM_IS_POSIX_BASED
    strcat(str, " -L \"");
    strcat(str, path);
    strcat(str, "\"");
#else
    strcat(str, " /LIBPATH:\"");
    strcat(str, path);
    strcat(str, "\"");
#endif
    p = strlist_next(p);
  }
}

static const char* suffix_filename(const char* dir, const char* file,
  const char* extension)
{
  // Copy to a string with space for a suffix.
  size_t len = strlen(dir) + strlen(file) + strlen(extension) + 3;
  VLA(char, filename, len + 1);

  // Start with no suffix.
  snprintf(filename, len, "%s/%s%s", dir, file, extension);
  int suffix = 0;

  while(suffix < 100)
  {
    struct stat s;
    int err = stat(filename, &s);

    if((err == -1) || !S_ISDIR(s.st_mode))
      break;

    snprintf(filename, len, "%s/%s%d%s", dir, file, ++suffix, extension);
  }

  if(suffix >= 100)
  {
    errorf(NULL, "couldn't pick an unused file name");
    return NULL;
  }

  return stringtab(filename);
}

static bool codegen_finalise(ast_t* program, compile_t* c, pass_opt_t* opt,
  pass_id pass_limit)
{
  // Finalise the function passes.
  LLVMFinalizeFunctionPassManager(c->fpm);

  if(opt->release)
  {
    printf("Optimising\n");

    // Module pass manager.
    LLVMRunPassManager(mpm, c->module);

    // LTO pass manager.
    if(!c->library)
      LLVMRunPassManager(lpm, c->module);
  }

  // Allocate on the stack instead of the heap where possible.
  stack_alloc(c);

#ifndef NDEBUG
  printf("Verifying\n");

  char* msg;

  if(LLVMVerifyModule(c->module, LLVMPrintMessageAction, &msg) != 0)
  {
    errorf(NULL, "module verification failed: %s", msg);
    LLVMDisposeMessage(msg);
    return false;
  }
#endif

  /*
   * Could store the pony runtime as a bitcode file. Build an executable by
   * amalgamating the program and the runtime.
   *
   * For building a library, could generate a .o without the runtime in it. The
   * user then has to link both the .o and the runtime. Would need a flag for
   * PIC or not PIC. Could even generate a .a and maybe a .so/.dll.
   */
  if(pass_limit == PASS_LLVM_IR)
  {
    const char* file_o = suffix_filename(opt->output, c->filename, ".ll");
    printf("Writing %s\n", file_o);
    char* err;

    if(LLVMPrintModuleToFile(c->module, file_o, &err) != 0)
    {
      errorf(NULL, "couldn't write IR to %s: %s", file_o, err);
      LLVMDisposeMessage(err);
      return false;
    }

    return true;
  }

  if(pass_limit == PASS_BITCODE)
  {
    const char* file_o = suffix_filename(opt->output, c->filename, ".bc");
    printf("Writing %s\n", file_o);

    if(LLVMWriteBitcodeToFile(c->module, file_o) != 0)
    {
      errorf(NULL, "couldn't write bitcode to %s", file_o);
      return false;
    }

    return true;
  }

  LLVMCodeGenFileType fmt;
  const char* file_o;

  if(pass_limit == PASS_ASM)
  {
    fmt = LLVMAssemblyFile;
    file_o = suffix_filename(opt->output, c->filename, ".s");
  } else {
    fmt = LLVMObjectFile;
#ifdef PLATFORM_IS_WINDOWS
    file_o = suffix_filename(opt->output, c->filename, ".obj");
#else
    file_o = suffix_filename(opt->output, c->filename, ".o");
#endif
  }

  printf("Writing %s\n", file_o);
  char* err;

  if(LLVMTargetMachineEmitToFile(
      c->machine, c->module, (char*)file_o, fmt, &err) != 0
    )
  {
    errorf(NULL, "couldn't create file: %s", err);
    LLVMDisposeMessage(err);
    return false;
  }

  if(pass_limit < PASS_ALL)
    return true;

  if(c->library)
  {
    // TODO:
    return true;
  }

#if defined(PLATFORM_IS_MACOSX)
  char* arch = strchr(opt->triple, '-');

  if(arch == NULL)
  {
    errorf(NULL, "couldn't determine architecture from %s", opt->triple);
    return false;
  }

  const char* file_exe = suffix_filename(opt->output, c->filename, "");
  printf("Linking %s\n", file_exe);

  size_t len = (arch - opt->triple);
  VLA(char, arch_buf, len + 1);
  memcpy(arch_buf, opt->triple, len);
  arch_buf[len] = '\0';
  program_lib_build_args(program, "", "", "-l", "");

  size_t ld_len = 128 + len + strlen(file_exe) + strlen(file_o) +
    link_path_length() + program_lib_arg_length(program);
  VLA(char, ld_cmd, ld_len);

  snprintf(ld_cmd, ld_len,
    "ld -execute -no_pie -dead_strip -arch %s -macosx_version_min 10.9.0 "
    "-o %s %s",
    arch_buf, file_exe, file_o
    );

  // User specified libraries go here, in any order.
  strcat(ld_cmd, program_lib_args(program));
  append_link_paths(ld_cmd);
  strcat(ld_cmd, " -lponyrt -lSystem");
#elif defined(PLATFORM_IS_LINUX)
  const char* file_exe = suffix_filename(opt->output, c->filename, "");
  printf("Linking %s\n", file_exe);
  program_lib_build_args(program, "--start-group ", "--end-group ", "-l", "");

  size_t ld_len = 256 + strlen(file_exe) + strlen(file_o) + link_path_length() +
    program_lib_arg_length(program);
  VLA(char, ld_cmd, ld_len);

  snprintf(ld_cmd, ld_len,
    "ld --eh-frame-hdr -m elf_x86_64 --hash-style=gnu "
    "-dynamic-linker /lib64/ld-linux-x86-64.so.2 "
    "-o %s "
    "/usr/lib/x86_64-linux-gnu/crt1.o "
    "/usr/lib/x86_64-linux-gnu/crti.o "
    "%s ",
    file_exe, file_o
    );

  append_link_paths(ld_cmd);

  // User specified libraries go here, surrounded with --start-group and
  // --end-group so that we don't have to determine an ordering.
  strcat(ld_cmd, program_lib_args(program));

  strcat(ld_cmd,
    " -lponyrt -lpthread -lm -lc "
    "/lib/x86_64-linux-gnu/libgcc_s.so.1 "
    "/usr/lib/x86_64-linux-gnu/crtn.o"
    );
#else
  vcvars_t vcvars;

  if(!vcvars_get(&vcvars))
  {
    errorf(NULL, "unable to link");
    return false;
  }

  const char* file_exe = suffix_filename(opt->output, c->filename, ".exe");
  printf("Linking %s\n", file_exe);
  program_lib_build_args(program, "", "", "", ".lib");

  size_t ld_len = 256 + strlen(file_exe) + strlen(file_o) + link_path_length() +
    vcvars_get_path_length(&vcvars) + program_lib_arg_length(program);
  VLA(char, ld_cmd, ld_len);

  snprintf(ld_cmd, ld_len,
    " /NOLOGO /NODEFAULTLIB /MACHINE:X64 "
    "/OUT:%s "
    "%s "
    "/LIBPATH:\"%s\" "
    "/LIBPATH:\"%s\" ",
    file_exe, file_o, vcvars.kernel32, vcvars.msvcrt
    );

  append_link_paths(ld_cmd);
  strcat(ld_cmd, program_lib_args(program));

  strcat(ld_cmd,
    " ponyrt.lib kernel32.lib msvcrt.lib"
    );
#endif

#if defined(PLATFORM_IS_POSIX_BASED)
  if(system(ld_cmd) != 0)
  {
    errorf(NULL, "unable to link");
    return false;
  }
#elif defined(PLATFORM_IS_WINDOWS)
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  DWORD code = 0;

  memset(&si, 0, sizeof(STARTUPINFO));

  if(!CreateProcess(TEXT(vcvars.link), TEXT(ld_cmd), NULL, NULL,
    FALSE, 0, NULL, NULL, &si, &pi))
  {
    errorf(NULL, "unable to invoke linker");
    return false;
  }

  WaitForSingleObject(pi.hProcess, INFINITE);
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hProcess);

  if(code != 0)
  {
    errorf(NULL, "unable to link");
    return false;
  }
#endif

  unlink(file_o);
  return true;
}

static void codegen_cleanup(compile_t* c)
{
  while(c->frame != NULL)
    pop_frame(c);

  LLVMDisposePassManager(c->fpm);
  LLVMDisposeBuilder(c->builder);
  LLVMDisposeModule(c->module);
  LLVMContextDispose(c->context);
  LLVMDisposeTargetMachine(c->machine);
  painter_free(c->painter);
}

bool codegen_init(pass_opt_t* opt)
{
  LLVMInitializeNativeTarget();
  LLVMInitializeAllTargets();
  LLVMInitializeAllTargetMCs();
  LLVMInitializeAllAsmPrinters();
  LLVMInitializeAllAsmParsers();
  LLVMEnablePrettyStackTrace();
  LLVMInstallFatalErrorHandler(codegen_fatal);

  LLVMPassRegistryRef passreg = LLVMGetGlobalPassRegistry();
  LLVMInitializeCore(passreg);
  LLVMInitializeTransformUtils(passreg);
  LLVMInitializeScalarOpts(passreg);
  LLVMInitializeObjCARCOpts(passreg);
  LLVMInitializeVectorization(passreg);
  LLVMInitializeInstCombine(passreg);
  LLVMInitializeIPO(passreg);
  LLVMInitializeInstrumentation(passreg);
  LLVMInitializeAnalysis(passreg);
  LLVMInitializeIPA(passreg);
  LLVMInitializeCodeGen(passreg);
  LLVMInitializeTarget(passreg);

  // Default triple, cpu and features.
  if(opt->triple != NULL)
    opt->triple = LLVMCreateMessage(opt->triple);
  else
    opt->triple = LLVMGetDefaultTargetTriple();

  if(opt->cpu != NULL)
    opt->cpu = LLVMCreateMessage(opt->cpu);
  else
    opt->cpu = LLVMGetHostCPUName();

  if(opt->features != NULL)
    opt->features = LLVMCreateMessage(opt->features);
  else
    opt->features = LLVMCreateMessage("");

  LLVMTargetMachineRef machine = make_machine(opt);

  if(machine == NULL)
  {
    errorf(NULL, "couldn't create target machine");
    return false;
  }

  LLVMTargetDataRef target_data = LLVMGetTargetMachineData(machine);

  // Pass manager builder.
  pmb = LLVMPassManagerBuilderCreate();
  LLVMPassManagerBuilderSetOptLevel(pmb, opt->release ? 3 : 0);

  if(opt->release)
    LLVMPassManagerBuilderUseInlinerWithThreshold(pmb, 275);

  // Module pass manager.
  mpm = LLVMCreatePassManager();
  LLVMAddTargetData(target_data, mpm);
  LLVMPassManagerBuilderPopulateModulePassManager(pmb, mpm);

  // LTO pass manager.
  lpm = LLVMCreatePassManager();
  LLVMAddTargetData(target_data, lpm);
  LLVMPassManagerBuilderPopulateLTOPassManager(pmb, lpm, true, true);

  LLVMDisposeTargetMachine(machine);
  return true;
}

void codegen_shutdown(pass_opt_t* opt)
{
  LLVMDisposePassManager(mpm);
  LLVMDisposePassManager(lpm);
  LLVMPassManagerBuilderDispose(pmb);

  LLVMDisposeMessage(opt->triple);
  LLVMDisposeMessage(opt->cpu);
  LLVMDisposeMessage(opt->features);

  LLVMShutdown();
}

bool codegen(ast_t* program, pass_opt_t* opt, pass_id pass_limit)
{
  printf("Generating\n");

  compile_t c;
  memset(&c, 0, sizeof(compile_t));

  init_module(&c, program, opt);
  init_runtime(&c);
  //dwarf_init(&c);
  genprim_builtins(&c);

  bool ok;

  if(c.library)
    ok = codegen_library(&c, program);
  else
    ok = codegen_program(&c, program);

  if(ok)
  {
    //ok = dwarf_finalise(c.dwarf);
    ok &= codegen_finalise(program, &c, opt, pass_limit);
  }

  //dwarf_cleanup(&c.dwarf);
  codegen_cleanup(&c);
  return ok;
}

LLVMValueRef codegen_addfun(compile_t*c, const char* name, LLVMTypeRef type)
{
  // Add the function and set the calling convention.
  LLVMValueRef fun = LLVMAddFunction(c->module, name, type);

  if(!c->library)
    LLVMSetFunctionCallConv(fun, GEN_CALLCONV);

  if(c->no_restrict)
    return fun;

  // Set the noalias attribute on all arguments. This is fortran-like semantics
  // for parameter aliasing, similar to C restrict.
  LLVMValueRef arg = LLVMGetFirstParam(fun);

  while(arg != NULL)
  {
    LLVMTypeRef type = LLVMTypeOf(arg);

    if(LLVMGetTypeKind(type) == LLVMPointerTypeKind)
      LLVMAddAttribute(arg, LLVMNoAliasAttribute);

    arg = LLVMGetNextParam(arg);
  }

  return fun;
}

void codegen_startfun(compile_t* c, LLVMValueRef fun)
{
  compile_frame_t* frame = push_frame(c);

  frame->fun = fun;
  frame->restore_builder = LLVMGetInsertBlock(c->builder);

  if(LLVMCountBasicBlocks(fun) == 0)
  {
    LLVMBasicBlockRef block = codegen_block(c, "entry");
    LLVMPositionBuilderAtEnd(c->builder, block);
  }
}

void codegen_pausefun(compile_t* c)
{
  if(c->frame->restore_builder != NULL)
    LLVMPositionBuilderAtEnd(c->builder, c->frame->restore_builder);

  pop_frame(c);
}

void codegen_finishfun(compile_t* c)
{
  compile_frame_t* frame = c->frame;

#ifndef NDEBUG
  if(LLVMVerifyFunction(frame->fun, LLVMPrintMessageAction) == 0)
  {
    LLVMRunFunctionPassManager(c->fpm, frame->fun);
  } else {
    LLVMDumpValue(frame->fun);
  }
#else
  LLVMRunFunctionPassManager(c->fpm, frame->fun);
#endif

  if(c->frame->restore_builder != NULL)
    LLVMPositionBuilderAtEnd(c->builder, c->frame->restore_builder);

  pop_frame(c);
}

void codegen_pushloop(compile_t* c, LLVMBasicBlockRef continue_target,
  LLVMBasicBlockRef break_target)
{
  compile_frame_t* frame = push_frame(c);

  frame->fun = frame->prev->fun;
  frame->restore_builder = frame->prev->restore_builder;
  frame->break_target = break_target;
  frame->continue_target = continue_target;
  frame->invoke_target = frame->prev->invoke_target;
}

void codegen_poploop(compile_t* c)
{
  pop_frame(c);
}

void codegen_pushtry(compile_t* c, LLVMBasicBlockRef invoke_target)
{
  compile_frame_t* frame = push_frame(c);

  frame->fun = frame->prev->fun;
  frame->restore_builder = frame->prev->restore_builder;
  frame->break_target = frame->prev->break_target;
  frame->continue_target = frame->prev->continue_target;
  frame->invoke_target = invoke_target;
}

void codegen_poptry(compile_t* c)
{
  pop_frame(c);
}

LLVMValueRef codegen_fun(compile_t* c)
{
  return c->frame->fun;
}

LLVMBasicBlockRef codegen_block(compile_t* c, const char* name)
{
  return LLVMAppendBasicBlockInContext(c->context, c->frame->fun, name);
}

LLVMValueRef codegen_call(compile_t* c, LLVMValueRef fun, LLVMValueRef* args,
  size_t count)
{
  LLVMValueRef result = LLVMBuildCall(c->builder, fun, args, (int)count, "");

  if(!c->library)
    LLVMSetInstructionCallConv(result, GEN_CALLCONV);

  return result;
}
