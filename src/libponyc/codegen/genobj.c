#include "genobj.h"

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

const char* genobj(compile_t* c)
{
  if(c->per_module_count > 0)
  {
    for(size_t i = 0; i < c->per_module_count; i++)
    {
      const char* pkg_sym = c->per_module_states[i].package_symbol;
      char suffix[256];
      snprintf(suffix, sizeof(suffix), ".%s.ll",
        (pkg_sym != NULL) ? pkg_sym : "main");
      const char* file_o = suffix_filename(c, c->opt->output, "",
        c->filename, suffix);

      if(!write_ir(c, c->per_module_states[i].module, file_o))
        return NULL;
    }

    return c->filename;
  }

  const char* file_o = suffix_filename(c, c->opt->output, "", c->filename,
    ".ll");

  if(!write_ir(c, c->module, file_o))
    return NULL;

  return file_o;
}
