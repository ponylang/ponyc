#include "codegen.h"
#include "genprim.h"
#include "genname.h"
#include "gentype.h"
#include "gendesc.h"
#include "genfun.h"
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
  type = LLVMStructCreateNamed(LLVMGetGlobalContext(), "$object");
  c->object_ptr = LLVMPointerType(type, 0);

  // padding required in an actor between the descriptor and fields
  c->actor_pad = LLVMArrayType(LLVMInt8Type(), 265);

  // trace
  // void (*)($object*)
  params[0] = c->object_ptr;
  c->trace_type = LLVMFunctionType(LLVMVoidType(), params, 1, false);
  c->trace_fn = LLVMPointerType(c->trace_type, 0);

  // dispatch
  // void (*)($object*, i8*)
  params[0] = c->object_ptr;
  params[1] = c->void_ptr;
  c->dispatch_fn = LLVMPointerType(
    LLVMFunctionType(LLVMVoidType(), params, 2, false), 0);

  // void (*)($object*)
  params[0] = c->object_ptr;
  c->final_fn = LLVMPointerType(
    LLVMFunctionType(LLVMVoidType(), params, 1, false), 0);

  // descriptor
  c->descriptor_type = gendesc_type(c, genname_descriptor(NULL), 0);
  c->descriptor_ptr = LLVMPointerType(c->descriptor_type, 0);

  // define object
  params[0] = c->descriptor_ptr;
  LLVMStructSetBody(type, params, 1, false);

  // $object* pony_create($desc*)
  params[0] = c->descriptor_ptr;
  type = LLVMFunctionType(c->object_ptr, params, 1, false);
  LLVMAddFunction(c->module, "pony_create", type);

  // void pony_sendv($object*, i8*);
  params[0] = c->object_ptr;
  params[1] = c->void_ptr;
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

  // i8* pony_alloc_msg(i32, i32)
  params[0] = LLVMInt32Type();
  params[1] = LLVMInt32Type();
  type = LLVMFunctionType(c->void_ptr, params, 2, false);
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

  // int pony_start(i32, i8**, $object*, i1)
  params[0] = LLVMInt32Type();
  params[1] = LLVMPointerType(c->void_ptr, 0);
  params[2] = c->object_ptr;
  params[3] = LLVMInt1Type();
  type = LLVMFunctionType(LLVMInt32Type(), params, 4, false);
  LLVMAddFunction(c->module, "pony_start", type);

  // void pony_throw()
  type = LLVMFunctionType(LLVMVoidType(), NULL, 0, true);
  LLVMAddFunction(c->module, "pony_throw", type);

  // i32 pony_personality(...)
  type = LLVMFunctionType(LLVMInt32Type(), NULL, 0, true);
  c->personality = LLVMAddFunction(c->module, "pony_personality", type);
}

static void codegen_main(compile_t* c, gentype_t* g)
{
  LLVMTypeRef params[2];
  params[0] = LLVMInt32Type();
  params[1] = LLVMPointerType(LLVMPointerType(LLVMInt8Type(), 0), 0);

  LLVMTypeRef ftype = LLVMFunctionType(LLVMInt32Type(), params, 2, false);
  LLVMValueRef func = LLVMAddFunction(c->module, "main", ftype);

  codegen_startfun(c, func);

  LLVMValueRef argc = LLVMGetParam(func, 0);
  LLVMSetValueName(argc, "argc");

  LLVMValueRef argv = LLVMGetParam(func, 1);
  LLVMSetValueName(argv, "argv");

  // TODO: create the main actor, start the pony runtime
  LLVMBuildRet(c->builder, argc);

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
  PONY_VL_ARRAY(char, buffer, len + 4);
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

  LLVMBasicBlockRef block = LLVMAppendBasicBlock(fun, "entry");
  LLVMPositionBuilderAtEnd(c->builder, block);
}

bool codegen_finishfun(compile_t* c)
{
  compile_context_t* context = c->context;

  if(LLVMVerifyFunction(context->fun, LLVMPrintMessageAction) != 0)
  {
    errorf(NULL, "function verification failed");
    pop_context(c);
    return false;
  }

  LLVMRunFunctionPassManager(c->fpm, context->fun);

  pop_context(c);
  return true;
}
