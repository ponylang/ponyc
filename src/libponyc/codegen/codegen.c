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

#include <llvm-c/Target.h>
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

static void codegen_main(compile_t* c, gentype_t* g)
{
  // TODO: Calculate the index of create().
  size_t index = 0;

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
  args[0] = LLVMConstInt(LLVMInt32Type(), index, false);
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
  c->painter = painter_create();
  painter_colour(c->painter, program);

  // the name of the first package is the name of the program
  c->filename = package_filename(ast_child(program));

  LLVMPassRegistryRef passreg = LLVMGetGlobalPassRegistry();
  LLVMInitializeCore(passreg);
  LLVMInitializeNativeTarget();
  LLVMEnablePrettyStackTrace();
  LLVMInstallFatalErrorHandler(codegen_fatal);

  // create a module
  c->module = LLVMModuleCreateWithName(c->filename);

  // function pass manager
  c->fpm = LLVMCreateFunctionPassManagerForModule(c->module);
  LLVMAddTargetData(LLVMCreateTargetData(LLVMGetDataLayout(c->module)), c->fpm);

  c->pmb = LLVMPassManagerBuilderCreate();
  LLVMPassManagerBuilderSetOptLevel(c->pmb, opt);
  LLVMPassManagerBuilderPopulateFunctionPassManager(c->pmb, c->fpm);

  LLVMInitializeFunctionPassManager(c->fpm);

  // IR builder
  c->builder = LLVMCreateBuilder();

  // target data
  c->target = LLVMCreateTargetData(LLVMGetDataLayout(c->module));

  // empty context stack
  c->context = NULL;
}

static bool codegen_finalise(compile_t* c)
{
  // finalise the function passes
  LLVMFinalizeFunctionPassManager(c->fpm);

  // module pass manager
  LLVMPassManagerRef mpm = LLVMCreatePassManager();
  LLVMPassManagerBuilderPopulateModulePassManager(c->pmb, mpm);
  LLVMRunPassManager(mpm, c->module);
  LLVMDisposePassManager(mpm);

  char* msg;

  if(LLVMVerifyModule(c->module, LLVMPrintMessageAction, &msg) != 0)
  {
    errorf(NULL, "module verification failed: %s", msg);
    LLVMDisposeMessage(msg);
    return false;
  }

  // generates bitcode. llc turns bitcode into assembly. llvm-mc turns
  // assembly into an object file. still need to link the object file with the
  // pony runtime and any other C libraries needed.
  size_t len = strlen(c->filename);
  VLA(char, buffer, len + 4);
  memcpy(buffer, c->filename, len);
  memcpy(buffer + len, ".bc", 4);

  LLVMWriteBitcodeToFile(c->module, buffer);
  printf("=== Compiled %s ===\n", buffer);

  return true;
}

static void codegen_cleanup(compile_t* c, bool print_llvm)
{
  while(c->context != NULL)
    pop_context(c);

  if(print_llvm)
    LLVMDumpModule(c->module);

  LLVMDisposeTargetData(c->target);
  LLVMDisposeBuilder(c->builder);
  LLVMDisposePassManager(c->fpm);
  LLVMDisposeModule(c->module);
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
    ok = codegen_finalise(&c);

  codegen_cleanup(&c, print_llvm);
  return ok;
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
