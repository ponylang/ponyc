#include "genobj.h"
#include <llvm-c/BitWriter.h>

static bool write_ir(compile_t* c, LLVMModuleRef module, const char* file_o)
{
  errors_t* errors = c->opt->check.errors;

  if(c->opt->verbosity >= VERBOSITY_MINIMAL)
    fprintf(stderr, "Writing %s\n", file_o);

  char* err;

  if(LLVMPrintModuleToFile(module, file_o, &err) != 0)
  {
    errorf(errors, NULL, "couldn't write IR to %s: %s", file_o, err);
    LLVMDisposeMessage(err);
    return false;
  }

  return true;
}

static bool write_bitcode(compile_t* c, LLVMModuleRef module,
  const char* file_o)
{
  errors_t* errors = c->opt->check.errors;

  if(c->opt->verbosity >= VERBOSITY_MINIMAL)
    fprintf(stderr, "Writing %s\n", file_o);

  if(LLVMWriteBitcodeToFile(module, file_o) != 0)
  {
    errorf(errors, NULL, "couldn't write bitcode to %s", file_o);
    return false;
  }

  return true;
}

const char* genobj(compile_t* c)
{
  errors_t* errors = c->opt->check.errors;
  const char* ext = (c->opt->limit == PASS_LLVM_IR) ? ".ll" : ".bc";

  if(c->opt->limit != PASS_LLVM_IR && c->opt->limit != PASS_BITCODE)
  {
    errorf(errors, NULL, "unexpected pass limit in genobj");
    return NULL;
  }

  if(c->per_module_count > 0)
  {
    for(size_t i = 0; i < c->per_module_count; i++)
    {
      const char* pkg_sym = c->per_module_states[i].package_symbol;
      char suffix[256];
      snprintf(suffix, sizeof(suffix), ".%s%s",
        (pkg_sym != NULL) ? pkg_sym : "main", ext);
      const char* file_o = suffix_filename(c, c->opt->output, "",
        c->filename, suffix);

      bool ok;
      if(c->opt->limit == PASS_LLVM_IR)
        ok = write_ir(c, c->per_module_states[i].module, file_o);
      else
        ok = write_bitcode(c, c->per_module_states[i].module, file_o);

      if(!ok)
        return NULL;
    }

    return c->filename;
  }

  const char* file_o = suffix_filename(c, c->opt->output, "", c->filename,
    ext);

  bool ok;
  if(c->opt->limit == PASS_LLVM_IR)
    ok = write_ir(c, c->module, file_o);
  else
    ok = write_bitcode(c, c->module, file_o);

  return ok ? file_o : NULL;
}
