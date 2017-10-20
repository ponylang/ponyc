#include "ponyc.h"
#include "ast/error.h"
#include "codegen/codegen.h"
#include "pkg/package.h"

bool ponyc_init(pass_opt_t* options)
{
  if(!codegen_llvm_init())
    return false;

  if(!codegen_pass_init(options))
    return false;

  if(!package_init(options))
    return false;

  return true;
}

void ponyc_shutdown(pass_opt_t* options)
{
  errors_print(options->check.errors);
  package_done(options);
  codegen_pass_cleanup(options);
  codegen_llvm_shutdown();
  stringtab_done();
}
