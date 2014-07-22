#include "codegen.h"
#include "gentype.h"
#include "genfun.h"
#include "../pass/names.h"
#include "../pkg/package.h"
#include "../ds/stringtab.h"
#include <llvm-c/Target.h>
#include <llvm-c/BitWriter.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

static void codegen_runtime(compile_t* c)
{
  LLVMTypeRef type;
  LLVMTypeRef params[7];

  // i8*
  c->void_ptr = LLVMPointerType(LLVMInt8Type(), 0);

  // pony_actor_t
  type = LLVMStructCreateNamed(LLVMGetGlobalContext(), "pony_actor_t");
  c->actor_ptr = LLVMPointerType(type, 0);

  // void (*)(i8*)
  params[0] = c->void_ptr;
  c->trace_type = LLVMFunctionType(LLVMVoidType(), params, 1, false);
  c->trace_fn = LLVMPointerType(c->trace_type, 0);

  // pony_type_t
  LLVMTypeRef pony_type = LLVMStructCreateNamed(LLVMGetGlobalContext(),
    "pony_type_t");
  params[0] = LLVMInt64Type();
  params[1] = c->trace_fn;
  params[2] = c->trace_fn;
  params[3] = c->trace_fn;
  LLVMStructSetBody(pony_type, params, 4, false);
  LLVMTypeRef pony_type_ptr = LLVMPointerType(pony_type, 0);

  // pony_msg_t
  type = LLVMStructCreateNamed(LLVMGetGlobalContext(), "pony_msg_t");
  params[0] = LLVMInt32Type();
  params[1] = LLVMArrayType(pony_type_ptr, 6);
  LLVMStructSetBody(type, params, 2, false);
  LLVMTypeRef pony_msg_ptr = LLVMPointerType(type, 0);

  // pony_msg_t* (*)(i64)
  params[0] = LLVMInt64Type();
  LLVMTypeRef msg_fn = LLVMPointerType(
    LLVMFunctionType(pony_msg_ptr, params, 1, false), 0);

  // void (*)(pony_actor_t*, i8*, i64, i32, 64)
  params[0] = c->actor_ptr;
  params[1] = c->void_ptr;
  params[2] = LLVMInt64Type();
  params[3] = LLVMInt32Type();
  params[4] = LLVMInt64Type();
  c->dispatch_fn = LLVMPointerType(
    LLVMFunctionType(LLVMVoidType(), params, 5, false), 0);

  // void (*)(void*)
  params[0] = c->void_ptr;
  c->final_fn = LLVMPointerType(
    LLVMFunctionType(LLVMVoidType(), params, 1, false), 0);

  type = LLVMStructCreateNamed(LLVMGetGlobalContext(), "$desc");
  params[0] = c->trace_fn; // trace
  params[1] = c->trace_fn; // serialise
  params[2] = c->trace_fn; // deserialise
  params[3] = c->dispatch_fn; // dispatch
  params[4] = c->final_fn; // finalise
  params[5] = c->void_ptr; // trait list
  params[6] = LLVMArrayType(c->void_ptr, 0); // vtable
  LLVMStructSetBody(type, params, 7, false);
  c->descriptor_ptr = LLVMPointerType(type, 0);

  type = LLVMStructCreateNamed(LLVMGetGlobalContext(), "$object");
  params[0] = c->descriptor_ptr;
  LLVMStructSetBody(type, params, 1, false);
  c->object_ptr = LLVMPointerType(type, 0);

  // pony_actor_type_t
  type = LLVMStructCreateNamed(LLVMGetGlobalContext(), "pony_actor_type_t");
  params[0] = LLVMInt32Type();
  params[1] = pony_type;
  params[2] = msg_fn;
  params[3] = c->dispatch_fn;
  params[4] = c->final_fn;
  LLVMStructSetBody(type, params, 5, false);
  LLVMTypeRef pony_actor_type_ptr = LLVMPointerType(type, 0);

  // pony_actor_t* pony_create(pony_actor_type_t*)
  params[0] = pony_actor_type_ptr;
  type = LLVMFunctionType(c->actor_ptr, params, 1, false);
  LLVMAddFunction(c->module, "pony_create", type);

  // void pony_set(i8*)
  params[0] = c->void_ptr;
  type = LLVMFunctionType(LLVMVoidType(), params, 1, false);
  LLVMAddFunction(c->module, "pony_set", type);

  // void pony_sendv(pony_actor_t*, i64, i32, i64*);
  params[0] = c->actor_ptr;
  params[1] = LLVMInt64Type();
  params[2] = LLVMInt32Type();
  params[3] = LLVMPointerType(LLVMInt64Type(), 0);
  type = LLVMFunctionType(LLVMVoidType(), params, 4, false);
  LLVMAddFunction(c->module, "pony_sendv", type);

  // i8* pony_alloc(i64)
  params[0] = LLVMInt64Type();
  type = LLVMFunctionType(c->void_ptr, params, 1, false);
  LLVMAddFunction(c->module, "pony_alloc", type);

  // void pony_trace(c->object_ptr)
  params[0] = c->object_ptr;
  type = LLVMFunctionType(LLVMVoidType(), params, 1, false);
  LLVMAddFunction(c->module, "pony_trace", type);

  // void pony_traceactor(pony_actor_t*)
  params[0] = c->actor_ptr;
  type = LLVMFunctionType(LLVMVoidType(), params, 1, false);
  LLVMAddFunction(c->module, "pony_traceactor", type);

  // void pony_traceobject(c->object_ptr, c->trace_fn*)
  params[0] = c->object_ptr;
  params[1] = c->trace_fn;
  type = LLVMFunctionType(LLVMVoidType(), params, 2, false);
  LLVMAddFunction(c->module, "pony_traceobject", type);

  // int pony_start(int, i8**, pony_actor_t*, bool)
  params[0] = LLVMInt32Type();
  params[1] = LLVMPointerType(c->void_ptr, 0);
  params[2] = c->actor_ptr;
  params[3] = LLVMInt1Type();
  type = LLVMFunctionType(LLVMInt32Type(), params, 4, false);
  LLVMAddFunction(c->module, "pony_start", type);
}

static bool codegen_main(compile_t* c, LLVMTypeRef type)
{
  LLVMTypeRef params[2];
  params[0] = LLVMInt32Type();
  params[1] = LLVMPointerType(LLVMPointerType(LLVMInt8Type(), 0), 0);

  LLVMTypeRef ftype = LLVMFunctionType(LLVMInt32Type(), params, 2, false);
  LLVMValueRef func = LLVMAddFunction(c->module, "main", ftype);

  LLVMValueRef argc = LLVMGetParam(func, 0);
  LLVMSetValueName(argc, "argc");

  LLVMValueRef argv = LLVMGetParam(func, 1);
  LLVMSetValueName(argv, "argv");

  LLVMBasicBlockRef block = LLVMAppendBasicBlock(func, "entry");
  LLVMPositionBuilderAtEnd(c->builder, block);

  // TODO: create the main actor, start the pony runtime
  LLVMBuildRet(c->builder, argc);

  return codegen_finishfun(c, func);
}

static bool codegen_program(compile_t* c, ast_t* program)
{
  // the first package is the main package. if it has a Main actor, this
  // is a program, otherwise this is a library.
  ast_t* package = ast_child(program);
  const char* main_actor = stringtab("Main");
  ast_t* m = ast_get(package, main_actor);

  if(m == NULL)
  {
    // TODO: how do we compile libraries?
    return true;
  }

  ast_t* ast = ast_from(m, TK_NOMINAL);
  ast_add(ast, ast_from(m, TK_NONE)); // ephemeral
  ast_add(ast, ast_from(m, TK_TAG)); // cap
  ast_add(ast, ast_from(m, TK_NONE)); // typeargs
  ast_add(ast, ast_from_string(m, main_actor)); // main
  ast_add(ast, package_id(package)); // initial package

  if(!names_nominal(package, &ast))
  {
    ast_free_unattached(ast);
    return false;
  }

  LLVMTypeRef type = gentype(c, ast);

  if(type == NULL)
  {
    ast_free_unattached(ast);
    return false;
  }

  LLVMValueRef fun = genfun(c, ast, stringtab("create"), NULL);
  ast_free_unattached(ast);

  if(fun == NULL)
    return false;

  return codegen_main(c, type);
}

static void codegen_init(compile_t* c, ast_t* program, int opt)
{
  // the name of the first package is the name of the program
  c->filename = package_filename(ast_child(program));

  LLVMPassRegistryRef passreg = LLVMGetGlobalPassRegistry();
  LLVMInitializeCore(passreg);
  LLVMInitializeNativeTarget();
  LLVMEnablePrettyStackTrace();

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
}

static bool codegen_finalise(compile_t* c, bool print_llvm)
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
    errorf(NULL, "module verification failed");
    LLVMDisposeMessage(msg);
    return false;
  }

  // generates bitcode. llc turns bitcode into assembly. llvm-mc turns
  // assembly into an object file. still need to link the object file with the
  // pony runtime and any other C libraries needed.
  size_t len = strlen(c->filename);
  char buffer[len + 4];
  memcpy(buffer, c->filename, len);
  memcpy(buffer + len, ".bc", 4);

  LLVMWriteBitcodeToFile(c->module, buffer);
  printf("=== Compiled %s ===\n", buffer);

  if(print_llvm)
    LLVMDumpModule(c->module);

  return true;
}

static void codegen_cleanup(compile_t* c)
{
  LLVMDisposeTargetData(c->target);
  LLVMDisposeBuilder(c->builder);
  LLVMDisposePassManager(c->fpm);
  LLVMDisposeModule(c->module);
  LLVMShutdown();
}

bool codegen(ast_t* program, int opt, bool print_llvm)
{
  compile_t c;
  codegen_init(&c, program, opt);
  codegen_runtime(&c);
  bool ok = codegen_program(&c, program);

  if(ok)
    ok = codegen_finalise(&c, print_llvm);

  codegen_cleanup(&c);
  return ok;
}

bool codegen_finishfun(compile_t* c, LLVMValueRef fun)
{
  if(LLVMVerifyFunction(fun, LLVMPrintMessageAction) != 0)
  {
    errorf(NULL, "function verification failed");
    return false;
  }

  LLVMRunFunctionPassManager(c->fpm, fun);
  return true;
}
