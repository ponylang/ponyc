#include "genobj.h"
#include "../debug/dwarf.h"
#include <llvm-c/BitWriter.h>

const char* genobj(compile_t* c)
{
  if(c->opt->release)
  {
    printf("Optimising\n");
    LLVMValueRef fun = LLVMGetFirstFunction(c->module);

    while(fun != NULL)
    {
      if(LLVMGetFirstBasicBlock(fun) != NULL)
        LLVMRunFunctionPassManager(c->fpm, fun);

      fun = LLVMGetNextFunction(fun);
    }

    // Finalise the function passes.
    LLVMFinalizeFunctionPassManager(c->fpm);

    // Module pass manager.
    LLVMRunPassManager(c->mpm, c->module);

    // LTO pass manager.
    if(!c->opt->library)
      LLVMRunPassManager(c->lpm, c->module);
  }

  // Allocate on the stack instead of the heap where possible.
  stack_alloc(c);

  // Finalise the DWARF info.
  dwarf_finalise(&c->dwarf);

#ifndef NDEBUG
  printf("Verifying\n");

  char* msg = NULL;

  if(LLVMVerifyModule(c->module, LLVMPrintMessageAction, &msg) != 0)
  {
    errorf(NULL, "module verification failed: %s", msg);
    LLVMDisposeMessage(msg);
    return NULL;
  }

  if(msg != NULL)
    LLVMDisposeMessage(msg);
#endif

  /*
   * Could store the pony runtime as a bitcode file. Build an executable by
   * amalgamating the program and the runtime.
   *
   * For building a library, could generate a .o without the runtime in it. The
   * user then has to link both the .o and the runtime. Would need a flag for
   * PIC or not PIC. Could even generate a .a and maybe a .so/.dll.
   */
  if(c->opt->limit == PASS_LLVM_IR)
  {
    const char* file_o = suffix_filename(c->opt->output, c->filename, ".ll");
    printf("Writing %s\n", file_o);
    char* err;

    if(LLVMPrintModuleToFile(c->module, file_o, &err) != 0)
    {
      errorf(NULL, "couldn't write IR to %s: %s", file_o, err);
      LLVMDisposeMessage(err);
      return NULL;
    }

    return file_o;
  }

  if(c->opt->limit == PASS_BITCODE)
  {
    const char* file_o = suffix_filename(c->opt->output, c->filename, ".bc");
    printf("Writing %s\n", file_o);

    if(LLVMWriteBitcodeToFile(c->module, file_o) != 0)
    {
      errorf(NULL, "couldn't write bitcode to %s", file_o);
      return NULL;
    }

    return file_o;
  }

  LLVMCodeGenFileType fmt;
  const char* file_o;

  if(c->opt->limit == PASS_ASM)
  {
    fmt = LLVMAssemblyFile;
    file_o = suffix_filename(c->opt->output, c->filename, ".s");
  } else {
    fmt = LLVMObjectFile;
#ifdef PLATFORM_IS_WINDOWS
    file_o = suffix_filename(c->opt->output, c->filename, ".obj");
#else
    file_o = suffix_filename(c->opt->output, c->filename, ".o");
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
    return NULL;
  }

  return file_o;
}
