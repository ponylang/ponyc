#include "codegen.h"
#include "../ds/stringtab.h"
#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/Transforms/PassManagerBuilder.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Analysis.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

typedef struct compile_t
{
  LLVMModuleRef module;
  LLVMBuilderRef builder;
  LLVMPassManagerRef fpm;
  LLVMPassManagerBuilderRef pmb;
} compile_t;

static bool codegen_module(compile_t* c, ast_t* ast)
{
  return true;
}

static bool codegen_package(compile_t* c, ast_t* ast)
{
  ast_t* child = ast_child(ast);

  while(child != NULL)
  {
    if(!codegen_module(c, ast))
      return false;

    child = ast_sibling(child);
  }

  return true;
}

static bool codegen_main(compile_t* c)
{
  LLVMTypeRef* params = malloc(sizeof(LLVMTypeRef) * 2);
  params[0] = LLVMInt32Type();
  params[1] = LLVMPointerType(LLVMPointerType(LLVMInt8Type(), 0), 0);

  LLVMTypeRef ftype = LLVMFunctionType(LLVMInt32Type(), params, 2, false);
  LLVMValueRef func = LLVMAddFunction(c->module, "main", ftype);
  LLVMSetLinkage(func, LLVMExternalLinkage);

  LLVMValueRef argc = LLVMGetParam(func, 0);
  LLVMSetValueName(argc, "argc");

  LLVMValueRef argv = LLVMGetParam(func, 1);
  LLVMSetValueName(argv, "argv");

  LLVMBasicBlockRef block = LLVMAppendBasicBlock(func, "entry");
  LLVMPositionBuilderAtEnd(c->builder, block);

  // TODO: create the main actor, start the pony runtime
  LLVMBuildRet(c->builder, argc);

  if(LLVMVerifyFunction(func, LLVMPrintMessageAction) != 0)
  {
    errorf(NULL, "couldn't generate main function");
    return false;
  }

  LLVMRunFunctionPassManager(c->fpm, func);
  return true;
}

static bool codegen_program(compile_t* c, ast_t* ast)
{
  ast_t* package = ast_child(ast);

  if(package == NULL)
  {
    ast_error(ast, "program has no packages");
    return false;
  }

  // the first package is the main package. if it has a Main actor, this
  // is a program, otherwise this is a library.
  ast_t* main = ast_get(package, stringtab("Main"));

  if((main != NULL) && !codegen_main(c))
    return false;

  while(package != NULL)
  {
    if(!codegen_package(c, package))
      return false;

    package = ast_sibling(package);
  }

  return true;
}

static void codegen_init(compile_t* c)
{
  LLVMPassRegistryRef passreg = LLVMGetGlobalPassRegistry();
  LLVMInitializeCore(passreg);
  LLVMInitializeNativeTarget();
  LLVMEnablePrettyStackTrace();

  // create a module
  c->module = LLVMModuleCreateWithName("output");

  // function pass manager
  c->fpm = LLVMCreateFunctionPassManagerForModule(c->module);
  LLVMAddTargetData(LLVMCreateTargetData(LLVMGetDataLayout(c->module)), c->fpm);

  c->pmb = LLVMPassManagerBuilderCreate();
  LLVMPassManagerBuilderSetOptLevel(c->pmb, 3);
  LLVMPassManagerBuilderPopulateFunctionPassManager(c->pmb, c->fpm);

  LLVMInitializeFunctionPassManager(c->fpm);

  // IR builder
  c->builder = LLVMCreateBuilder();
}

static void codegen_finalise(compile_t* c)
{
  // finalise the function passes
  LLVMFinalizeFunctionPassManager(c->fpm);

  // module pass manager
  LLVMPassManagerRef mpm = LLVMCreatePassManager();
  LLVMPassManagerBuilderPopulateModulePassManager(c->pmb, mpm);
  LLVMRunPassManager(mpm, c->module);
  LLVMDisposePassManager(mpm);

  // write the bitcode to a file. to generate an executable, we need to use
  // other tools. simplest might be to use clang, since it will both compile
  // the bitcode and link it with any C libraries we need, including the
  // runtime. we can also use it to generate a library. alternatively, it might
  // be possible to use LLVM-MC to get from bitcode to machine code, getting
  // to a .o file.
  LLVMWriteBitcodeToFile(c->module, "output.bc");
  // LLVMDumpModule(c->module);
}

static void codegen_cleanup(compile_t* c)
{
  LLVMDisposeBuilder(c->builder);
  LLVMDisposePassManager(c->fpm);
  LLVMDisposeModule(c->module);
  LLVMShutdown();
}

bool codegen(ast_t* program)
{
  compile_t c;
  codegen_init(&c);
  bool ok = codegen_program(&c, program);

  if(ok)
    codegen_finalise(&c);

  codegen_cleanup(&c);
  return ok;
}
