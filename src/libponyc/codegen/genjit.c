#include "genjit.h"
#include "genexe.h"
#include "genopt.h"
#include <llvm-c/ExecutionEngine.h>
#include <string.h>

static LLVMBool jit_init(compile_t* c, LLVMExecutionEngineRef* engine)
{
  struct LLVMMCJITCompilerOptions options;
  LLVMInitializeMCJITCompilerOptions(&options, sizeof(options));
  return LLVMCreateMCJITCompilerForModule(engine, c->module, &options,
    sizeof(options), NULL);
}

bool gen_jit_and_run(compile_t* c, int* exit_code, jit_symbol_t* symbols,
  size_t symbol_count)
{
  reach_type_t* t_main = reach_type_name(c->reach, "Main");
  reach_type_t* t_env = reach_type_name(c->reach, "Env");

  if((t_main == NULL) || (t_env == NULL))
    return false;

  LLVMValueRef main_fn = gen_main(c, t_main, t_env);

  if(!genopt(c, true))
    return false;

  if(LLVMVerifyModule(c->module, LLVMReturnStatusAction, NULL) != 0)
    return false;

  LLVMExecutionEngineRef engine = NULL;
  if(jit_init(c, &engine) != 0)
    return false;

  for(size_t i = 0; i < symbol_count; i++)
  {
    uint64_t address = LLVMGetGlobalValueAddress(engine, symbols[i].name);
    if(address == 0)
    {
      LLVMRemoveModule(engine, c->module, &c->module, NULL);
      LLVMDisposeExecutionEngine(engine);
      return false;
    }

    memcpy(symbols[i].address, (void*)(uintptr_t)address, symbols[i].size);
  }

  const char* argv[] = {"ponyjit", NULL};
  const char* envp = NULL;
  int rc = LLVMRunFunctionAsMain(engine, main_fn, 1, argv, &envp);
  if(exit_code != NULL)
    *exit_code = rc;

  LLVMRemoveModule(engine, c->module, &c->module, NULL);
  LLVMDisposeExecutionEngine(engine);

  return true;
}
