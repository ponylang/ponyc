#include "ponyc.h"
#include "ast/error.h"
#include "codegen/codegen.h"
#include "pkg/package.h"

bool ponyc_init(pass_opt_t *options)
{
  if (!codegen_init(options))
    return false;

  if (!package_init())
    return false;

  return true;
}

void ponyc_shutdown(pass_opt_t *options)
{
  print_errors();
  free_errors();
  package_done();
  codegen_shutdown(options);
  stringtab_done();
}
