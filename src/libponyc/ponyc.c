#include "ponyc.h"
#include "codegen/codegen.h"
#include "pkg/package.h"

bool ponyc_init(pass_opt_t* options)
{
  if(!codegen_llvm_init())
    return false;

  if(!codegen_pass_init(options))
    return false;

  if(!module_init(options))
    return false;

  return true;
}

void ponyc_shutdown(pass_opt_t* options)
{
  module_done(options);
  codegen_pass_cleanup(options);
  codegen_llvm_shutdown();
  // The interned-string table is freed by pass_opt_done, which the caller runs
  // after this; it must stay live through the cleanup above (error printing,
  // module teardown) since those still read interned strings.
}
