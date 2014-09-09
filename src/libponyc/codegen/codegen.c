#include "codegen.h"
#include "genprim.h"
#include "genname.h"
#include "gentype.h"
#include "gendesc.h"
#include "genfun.h"
#include "gencall.h"
#include "../pass/names.h"
#include "../pkg/package.h"
#include "../ast/error.h"
#include "../ds/stringtab.h"

#include <llvm-c/Initialization.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/BitWriter.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void codegen_fatal(const char* reason)
{
  print_errors();
}

static compile_context_t* push_context(compile_t* c)
{
  compile_context_t* context = (compile_context_t*)calloc(1,
    sizeof(compile_context_t));

  context->prev = c->context;
  c->context = context;

  return context;
}

static void pop_context(compile_t* c)
{
  compile_context_t* context = c->context;
  c->context = context->prev;

  if(context->restore_builder != NULL)
    LLVMPositionBuilderAtEnd(c->builder, context->restore_builder);

  free(context);
}

static void codegen_runtime(compile_t* c)
{
  LLVMTypeRef type;
  LLVMTypeRef params[4];

  // i8*
  c->void_ptr = LLVMPointerType(LLVMInt8Type(), 0);

  // forward declare object
  c->object_type = LLVMStructCreateNamed(LLVMGetGlobalContext(), "$object");
  c->object_ptr = LLVMPointerType(c->object_type, 0);

  // padding required in an actor between the descriptor and fields
  c->actor_pad = LLVMArrayType(LLVMInt8Type(), 265);

  // message
  params[0] = LLVMInt32Type();
  params[1] = LLVMInt32Type();
  c->msg_type = LLVMStructCreateNamed(LLVMGetGlobalContext(), "$message");
  c->msg_ptr = LLVMPointerType(c->msg_type, 0);
  LLVMStructSetBody(c->msg_type, params, 2, false);

  // trace
  // void (*)($object*)
  params[0] = c->object_ptr;
  c->trace_type = LLVMFunctionType(LLVMVoidType(), params, 1, false);
  c->trace_fn = LLVMPointerType(c->trace_type, 0);

  // dispatch
  // void (*)($object*, $message*)
  params[0] = c->object_ptr;
  params[1] = c->msg_ptr;
  c->dispatch_type = LLVMFunctionType(LLVMVoidType(), params, 2, false);
  c->dispatch_fn = LLVMPointerType(c->dispatch_type, 0);

  // void (*)($object*)
  params[0] = c->object_ptr;
  c->final_fn = LLVMPointerType(
    LLVMFunctionType(LLVMVoidType(), params, 1, false), 0);

  // descriptor
  c->descriptor_type = gendesc_type(c, genname_descriptor(NULL), 0);
  c->descriptor_ptr = LLVMPointerType(c->descriptor_type, 0);

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
  type = LLVMFunctionType(LLVMVoidType(), params, 2, false);
  LLVMAddFunction(c->module, "pony_sendv", type);

  // i8* pony_alloc(i64)
  params[0] = LLVMInt64Type();
  type = LLVMFunctionType(c->void_ptr, params, 1, false);
  LLVMAddFunction(c->module, "pony_alloc", type);

  // i8* pony_realloc(i8*, i64)
  params[0] = c->void_ptr;
  params[1] = LLVMInt64Type();
  type = LLVMFunctionType(c->void_ptr, params, 2, false);
  LLVMAddFunction(c->module, "pony_realloc", type);

  // $message* pony_alloc_msg(i32, i32)
  params[0] = LLVMInt32Type();
  params[1] = LLVMInt32Type();
  type = LLVMFunctionType(c->msg_ptr, params, 2, false);
  LLVMAddFunction(c->module, "pony_alloc_msg", type);

  // void pony_trace(i8*)
  params[0] = c->void_ptr;
  type = LLVMFunctionType(LLVMVoidType(), params, 1, false);
  LLVMAddFunction(c->module, "pony_trace", type);

  // void pony_traceactor($object*)
  params[0] = c->object_ptr;
  type = LLVMFunctionType(LLVMVoidType(), params, 1, false);
  LLVMAddFunction(c->module, "pony_traceactor", type);

  // void pony_traceobject($object*, trace_fn)
  params[0] = c->object_ptr;
  params[1] = c->trace_fn;
  type = LLVMFunctionType(LLVMVoidType(), params, 2, false);
  LLVMAddFunction(c->module, "pony_traceobject", type);

  // void pony_gc_send()
  type = LLVMFunctionType(LLVMVoidType(), NULL, 0, false);
  LLVMAddFunction(c->module, "pony_gc_send", type);

  // void pony_gc_recv()
  type = LLVMFunctionType(LLVMVoidType(), NULL, 0, false);
  LLVMAddFunction(c->module, "pony_gc_recv", type);

  // void pony_send_done()
  type = LLVMFunctionType(LLVMVoidType(), NULL, 0, false);
  LLVMAddFunction(c->module, "pony_send_done", type);

  // void pony_recv_done()
  type = LLVMFunctionType(LLVMVoidType(), NULL, 0, false);
  LLVMAddFunction(c->module, "pony_recv_done", type);

  // i32 pony_init(i32, i8**)
  params[0] = LLVMInt32Type();
  params[1] = LLVMPointerType(c->void_ptr, 0);
  type = LLVMFunctionType(LLVMInt32Type(), params, 2, false);
  LLVMAddFunction(c->module, "pony_init", type);

  // void pony_become($object*)
  params[0] = c->object_ptr;
  type = LLVMFunctionType(LLVMVoidType(), params, 1, false);
  LLVMAddFunction(c->module, "pony_become", type);

  // i32 pony_start(i32)
  params[0] = LLVMInt32Type();
  type = LLVMFunctionType(LLVMInt32Type(), params, 1, false);
  LLVMAddFunction(c->module, "pony_start", type);

  // void pony_throw()
  type = LLVMFunctionType(LLVMVoidType(), NULL, 0, false);
  LLVMAddFunction(c->module, "pony_throw", type);

  // i32 pony_personality_v0(...)
  type = LLVMFunctionType(LLVMInt32Type(), NULL, 0, true);
  c->personality = LLVMAddFunction(c->module, "pony_personality_v0", type);

  // i8* memcpy(i8*, i8*, i64)
  params[0] = c->void_ptr;
  params[1] = c->void_ptr;
  params[2] = LLVMInt64Type();
  type = LLVMFunctionType(c->void_ptr, params, 3, false);
  LLVMAddFunction(c->module, "memcpy", type);
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

        if(!strcmp(ast_name(id), name))
          return index;

        index++;
        break;
      }

      default: {}
    }
  }

  return -1;
}

static void codegen_main(compile_t* c, gentype_t* g)
{
  LLVMTypeRef params[2];
  params[0] = LLVMInt32Type();
  params[1] = LLVMPointerType(LLVMPointerType(LLVMInt8Type(), 0), 0);

  LLVMTypeRef ftype = LLVMFunctionType(LLVMInt32Type(), params, 2, false);
  LLVMValueRef func = LLVMAddFunction(c->module, "main", ftype);

  codegen_startfun(c, func);

  LLVMValueRef args[2];
  args[0] = LLVMGetParam(func, 0);
  LLVMSetValueName(args[0], "argc");

  args[1] = LLVMGetParam(func, 1);
  LLVMSetValueName(args[1], "argv");

  // Initialise the pony runtime with argc and argv, getting a new argc.
  args[0] = gencall_runtime(c, "pony_init", args, 2, "argc");

  // Create the main actor and become it.
  LLVMValueRef m = gencall_create(c, g);
  LLVMValueRef object = LLVMBuildBitCast(c->builder, m, c->object_ptr, "");
  gencall_runtime(c, "pony_become", &object, 1, "");

  // Create an Env on the main actor's heap.
  const char* env_name = "$1_Env";
  const char* env_create = genname_fun(env_name, "_create", NULL);
  args[0] = LLVMBuildZExt(c->builder, args[0], LLVMInt64Type(), "");
  LLVMValueRef env = gencall_runtime(c, env_create, args, 2, "env");
  LLVMSetInstructionCallConv(env, LLVMFastCallConv);

  // Create a type for the message.
  LLVMTypeRef f_params[4];
  f_params[0] = LLVMInt32Type();
  f_params[1] = LLVMInt32Type();
  f_params[2] = c->void_ptr;
  f_params[3] = LLVMTypeOf(env);
  LLVMTypeRef msg_type = LLVMStructType(f_params, 4, false);
  LLVMTypeRef msg_type_ptr = LLVMPointerType(msg_type, 0);

  // Allocate the message, setting its size and ID.
  args[1] = LLVMConstInt(LLVMInt32Type(), 0, false);
  args[0] = LLVMConstInt(LLVMInt32Type(), behaviour_index(g, "create"), false);
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
  LLVMValueRef zero = LLVMConstInt(LLVMInt32Type(), 0, false);
  LLVMValueRef rc = gencall_runtime(c, "pony_start", &zero, 1, "");

  // Return the runtime exit code.
  LLVMBuildRet(c->builder, rc);

  codegen_finishfun(c);

  // External linkage for main().
  LLVMSetLinkage(func, LLVMExternalLinkage);
}

static bool codegen_type(compile_t* c, ast_t* scope, const char* package,
  const char* name, gentype_t* g)
{
  ast_t* ast = ast_from(scope, TK_NOMINAL);
  ast_add(ast, ast_from(scope, TK_NONE)); // ephemeral
  ast_add(ast, ast_from(scope, TK_VAL)); // cap
  ast_add(ast, ast_from(scope, TK_NONE)); // typeargs
  ast_add(ast, ast_from_string(scope, name));
  ast_add(ast, ast_from_string(scope, package));

  bool ok = names_nominal(scope, &ast) && gentype(c, ast, g);
  ast_free_unattached(ast);

  return ok;
}

static bool codegen_program(compile_t* c, ast_t* program)
{
  // the first package is the main package. if it has a Main actor, this
  // is a program, otherwise this is a library.
  ast_t* package = ast_child(program);
  const char* main_actor = stringtab("Main");
  ast_t* m = (ast_t*)ast_get(package, main_actor);

  if(m == NULL)
  {
    // TODO: How do we compile libraries? By specifying C ABI entry points and
    // compiling reachable code from there.
    return true;
  }

  // Generate the Main actor.
  gentype_t g;

  if(!codegen_type(c, m, package_name(package), main_actor, &g))
    return false;

  codegen_main(c, &g);
  return true;
}

static void codegen_init(compile_t* c, ast_t* program, int opt)
{
  LLVMInitializeNativeTarget();
  LLVMInitializeAllTargets();
  LLVMInitializeAllTargetMCs();
  LLVMInitializeAllAsmPrinters();
  LLVMEnablePrettyStackTrace();
  LLVMInstallFatalErrorHandler(codegen_fatal);

  c->painter = painter_create();
  painter_colour(c->painter, program);

  // The name of the first package is the name of the program.
  c->filename = package_filename(ast_child(program));

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

  // Default triple.
  c->triple = LLVMGetDefaultTargetTriple();

  // Create a module.
  c->module = LLVMModuleCreateWithName(c->filename);

  // Set the target triple.
  LLVMSetTarget(c->module, c->triple);

  // Target data.
  c->target_data = LLVMCreateTargetData(LLVMGetDataLayout(c->module));

  // Pass manager builder.
  c->pmb = LLVMPassManagerBuilderCreate();
  LLVMPassManagerBuilderSetOptLevel(c->pmb, opt);
  LLVMPassManagerBuilderUseInlinerWithThreshold(c->pmb, 275);

  // Function pass manager.
  c->fpm = LLVMCreateFunctionPassManagerForModule(c->module);
  LLVMAddTargetData(c->target_data, c->fpm);
  LLVMPassManagerBuilderPopulateFunctionPassManager(c->pmb, c->fpm);
  LLVMInitializeFunctionPassManager(c->fpm);

  // IR builder.
  c->builder = LLVMCreateBuilder();

  // Empty context stack.
  c->context = NULL;
}

static size_t link_path_length()
{
  strlist_t* p = package_paths();
  size_t len = 0;

  while(p != NULL)
  {
    const char* path = strlist_data(p);
    len += strlen(path) + 3;
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
    strcat(str, " -L");
    strcat(str, path);
    p = strlist_next(p);
  }
}

static bool codegen_finalise(compile_t* c, int opt)
{
  // Finalise the function passes.
  LLVMFinalizeFunctionPassManager(c->fpm);

  // Module pass manager.
  LLVMPassManagerRef mpm = LLVMCreatePassManager();
  LLVMAddTargetData(c->target_data, mpm);
  LLVMPassManagerBuilderPopulateModulePassManager(c->pmb, mpm);
  LLVMRunPassManager(mpm, c->module);
  LLVMDisposePassManager(mpm);

  // LTO pass manager.
  LLVMPassManagerRef lpm = LLVMCreatePassManager();
  LLVMAddTargetData(c->target_data, lpm);
  LLVMPassManagerBuilderPopulateLTOPassManager(c->pmb, lpm, true, true);
  LLVMRunPassManager(lpm, c->module);
  LLVMDisposePassManager(lpm);

  char* msg;

  if(LLVMVerifyModule(c->module, LLVMPrintMessageAction, &msg) != 0)
  {
    errorf(NULL, "module verification failed: %s", msg);
    LLVMDisposeMessage(msg);
    return false;
  }

  /*
   * Could store the pony runtime as a bitcode file. Build an executable by
   * amalgamating the program and the runtime.
   *
   * For building a library, could generate a .o without the runtime in it. The
   * user then has to link both the .o and the runtime. Would need a flag for
   * PIC or not PIC. Could even generate a .a and maybe a .so/.dll.
   */

  // Pick an executable name.
  size_t len = strlen(c->filename);
  VLA(char, file_exe, len + 3);
  snprintf(file_exe, len + 3, "%s", c->filename);
  int suffix = 0;

  while(suffix < 100)
  {
    struct stat s;
    int err = stat(file_exe, &s);

    if((err == -1) || !S_ISDIR(s.st_mode))
      break;

    snprintf(file_exe, len + 3, "%s%d", c->filename, ++suffix);
  }

  if(suffix >= 100)
  {
    errorf(NULL, "couldn't pick a name for the executable");
    return false;
  }

  len = strlen(file_exe);

  // Generate an object file.
  VLA(char, file_o, len + 3);
  snprintf(file_o, len + 3, "%s.o", file_exe);

  LLVMTargetRef target;
  char* err;

  if(LLVMGetTargetFromTriple(c->triple, &target, &err) != 0)
  {
    errorf(NULL, "couldn't create target: %s", err);
    LLVMDisposeMessage(err);
    return false;
  }

  // TODO LINK: allow passing in the cpu and feature set
  // -mcpu=mycpu -mattr=+feature1,-feature2
  char* cpu = LLVMGetHostCPUName();

  LLVMTargetMachineRef machine = LLVMCreateTargetMachine(target, c->triple,
    cpu, "", opt, LLVMRelocStatic, LLVMCodeModelDefault);

  LLVMDisposeMessage(cpu);

  if(machine == NULL)
  {
    errorf(NULL, "couldn't create target machine");
    return false;
  }

  if(LLVMTargetMachineEmitToFile(machine, c->module, file_o, LLVMObjectFile,
    &err) != 0)
  {
    errorf(NULL, "couldn't create object file: %s", err);
    LLVMDisposeMessage(err);
    LLVMDisposeTargetMachine(machine);
    return false;
  }

  LLVMDisposeTargetMachine(machine);

  // Link the program.
#if defined(PLATFORM_IS_MACOSX)
  char* arch = strchr(c->triple, '-');

  if(arch == NULL)
  {
    errorf(NULL, "couldn't determine architecture from %s", c->triple);
    return false;
  }

  arch = strndup(c->triple, arch - c->triple);

  size_t ld_len = 128 + strlen(arch) + (len * 2) + link_path_length();
  VLA(char, ld_cmd, ld_len);

  snprintf(ld_cmd, ld_len,
    "ld -execute -arch %s -macosx_version_min 10.9.0 -o %s %s.o",
    arch, file_exe, file_exe
    );

  append_link_paths(ld_cmd);
  strcat(ld_cmd, " -lpony -lSystem");
  free(arch);

  if(system(ld_cmd) != 0)
  {
    errorf(NULL, "unable to link");
    return false;
  }

  unlink(file_o);
#elif defined(PLATFORM_IS_LINUX)
  size_t ld_len = 256 + (len * 2) + link_path_length();
  VLA(char, ld_cmd, ld_len);

  snprintf(ld_cmd, ld_len,
    "ld --eh-frame-hdr -m elf_x86_64 --hash-style=gnu "
    "-dynamic-linker /lib64/ld-linux-x86-64.so.2 "
    "-o %s "
    "/usr/lib/x86_64-linux-gnu/crt1.o "
    "/usr/lib/x86_64-linux-gnu/crti.o "
    "%s.o ",
    file_exe, file_exe
    );

  append_link_paths(ld_cmd);

  strcat(ld_cmd,
    " -lpony -lpthread -lc "
    "/lib/x86_64-linux-gnu/libgcc_s.so.1 "
    "/usr/lib/x86_64-linux-gnu/crtn.o"
    );

  if(system(ld_cmd) != 0)
  {
    errorf(NULL, "unable to link");
    return false;
  }

  unlink(file_o);
#else
  printf("Compiled %s, please link it by hand to libpony.a\n", file_o);
  return true;
#endif

  printf("=== Compiled %s ===\n", file_exe);
  return true;
}

static void codegen_cleanup(compile_t* c, bool print_llvm)
{
  while(c->context != NULL)
    pop_context(c);

  if(print_llvm)
    LLVMDumpModule(c->module);

  LLVMDisposePassManager(c->fpm);
  LLVMPassManagerBuilderDispose(c->pmb);

  LLVMDisposeBuilder(c->builder);
  LLVMDisposeTargetData(c->target_data);
  LLVMDisposeModule(c->module);
  LLVMDisposeMessage(c->triple);
  LLVMShutdown();
  painter_free(c->painter);
}

bool codegen(ast_t* program, int opt, bool print_llvm)
{
  compile_t c;
  codegen_init(&c, program, opt);
  codegen_runtime(&c);
  genprim_builtins(&c);
  bool ok = codegen_program(&c, program);

  if(ok)
    ok = codegen_finalise(&c, opt);

  codegen_cleanup(&c, print_llvm);
  return ok;
}

LLVMValueRef codegen_addfun(compile_t*c, const char* name, LLVMTypeRef type)
{
  LLVMValueRef fun = LLVMAddFunction(c->module, name, type);
  LLVMSetFunctionCallConv(fun, LLVMFastCallConv);
  return fun;
}

void codegen_startfun(compile_t* c, LLVMValueRef fun)
{
  compile_context_t* context = push_context(c);

  context->fun = fun;
  context->restore_builder = LLVMGetInsertBlock(c->builder);

  if(LLVMCountBasicBlocks(fun) == 0)
  {
    LLVMBasicBlockRef block = LLVMAppendBasicBlock(fun, "entry");
    LLVMPositionBuilderAtEnd(c->builder, block);
  }
}

void codegen_pausefun(compile_t* c)
{
  pop_context(c);
}

void codegen_finishfun(compile_t* c)
{
  compile_context_t* context = c->context;

  if(LLVMVerifyFunction(context->fun, LLVMPrintMessageAction) == 0)
  {
    LLVMRunFunctionPassManager(c->fpm, context->fun);
  } else {
    errorf(NULL, "function verification failed");
  }

  pop_context(c);
}
