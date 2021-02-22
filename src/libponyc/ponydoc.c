#include "codegen/codegen.h"
#include "pass/pass.h"
#include "pkg/package.h"

bool ponydoc_init(pass_opt_t* options, const char* pony_installation)
{
  // TODO STA:
  // ponyc does this here
  //if(!codegen_llvm_init())
  //  return false;
  //i don't think we need it but leaving for now

  if(!codegen_pass_init(options))
    return false;

  if(!package_init_lib(options, pony_installation))
    return false;

  return true;
}

ast_t* ponydoc_load(const char* path, const char* pony_installation)
{
  pass_opt_t opt;
  pass_opt_init(&opt);

  // Stop at trait pass as that's the last pass before doc gen
  opt.limit = PASS_TRAITS;
  // hardcode where we put any output
  opt.output = ".";
  // slow is fine, not sure it even matters to
  // the doc tool. I think only impacts passes after our limit
  opt.release = false;
  // hardcode the printing width
  opt.ast_print_width = 80;
  // TODO STA: do we need to set this if we are using an explicit pony
  // installation?
  opt.argv0 = "this shouldn't be used";

  ponydoc_init(&opt, pony_installation);

  ast_t* program = program_load(path, &opt);

  if (program == NULL)
    errors_print(opt.check.errors);

  return program;
}


