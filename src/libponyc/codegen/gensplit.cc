#include "gensplit.h"
#include "genopt.h"
#include "../ast/error.h"
#include "../ast/stringtab.h"

#include "llvm_config_begin.h"

#include <llvm/IR/Module.h>
#include <llvm/Analysis/ModuleSummaryAnalysis.h>
#include <llvm/Analysis/ProfileSummaryInfo.h>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/raw_ostream.h>

#include "llvm_config_end.h"

#include "../../libponyrt/mem/pool.h"

#include <string.h>

#ifdef PLATFORM_IS_WINDOWS
#  include <io.h>
#  define unlink_file _unlink
#else
#  include <unistd.h>
#  define unlink_file unlink
#endif

using namespace llvm;

static bool write_module_bitcode(compile_t* c, Module& mod,
  const char* file_bc, errors_t* errors)
{
  if(c->opt->verbosity >= VERBOSITY_MINIMAL)
    fprintf(stderr, "Writing %s\n", file_bc);

  std::error_code ec;
  raw_fd_ostream out(file_bc, ec);

  if(ec)
  {
    errorf(errors, NULL, "couldn't write bitcode to %s: %s",
      file_bc, ec.message().c_str());
    return false;
  }

  if(c->opt->fat_lto)
  {
    WriteBitcodeToFile(mod, out);
  } else {
    ProfileSummaryInfo PSI(mod);
    ModuleSummaryIndex index = buildModuleSummaryIndex(
      mod,
      /*GetBFICallback=*/nullptr,
      &PSI);

    WriteBitcodeToFile(mod, out,
      /*ShouldPreserveUseListOrder=*/false,
      &index,
      /*GenerateHash=*/true);
  }

  out.flush();

  if(out.has_error())
  {
    out.clear_error();
    errorf(errors, NULL, "error writing bitcode to %s", file_bc);
    unlink_file(file_bc);
    return false;
  }

  return true;
}

static bool emit_per_module_bitcode(compile_t* c, const char*** out_files,
  size_t* out_count)
{
  errors_t* errors = c->opt->check.errors;
  size_t count = c->per_module_count;

  if(c->opt->verbosity >= VERBOSITY_MINIMAL)
    fprintf(stderr, "Emitting %zu per-package modules\n", count);

  const char** files = (const char**)ponyint_pool_alloc_size(
    count * sizeof(const char*));

  for(size_t i = 0; i < count; i++)
  {
    char suffix[256];
    const char* pkg_sym = c->per_module_states[i].package_symbol;
    snprintf(suffix, sizeof(suffix), ".%s.bc",
      (pkg_sym != NULL) ? pkg_sym : "main");
    const char* file_bc = suffix_filename(c, c->opt->output, "",
      c->filename, suffix);

    Module& mod = *unwrap(c->per_module_states[i].module);
    mod.setSourceFileName(file_bc);

    // ThinLTO's renameModuleForThinLTO crashes on unnamed private globals.
    // Name them so the summary index can reference them; keep private
    // linkage so they stay module-local.
    unsigned anon_id = 0;
    for(auto& G : mod.globals())
    {
      if(!G.hasName() && G.hasLocalLinkage())
      {
        char name[128];
        snprintf(name, sizeof(name), "__pony_priv.%zu.g%u", i, anon_id++);
        G.setName(name);
      }
    }
    for(auto& F : mod.functions())
    {
      if(!F.hasName() && F.hasLocalLinkage())
      {
        char name[128];
        snprintf(name, sizeof(name), "__pony_priv.%zu.f%u", i, anon_id++);
        F.setName(name);
      }
    }

    if(!write_module_bitcode(c, mod, file_bc, errors))
    {
      for(size_t j = 0; j < i; j++)
        unlink_file(files[j]);

      ponyint_pool_free_size(count * sizeof(const char*), files);
      return false;
    }

    files[i] = file_bc;
  }

  *out_files = files;
  *out_count = count;
  return true;
}

bool split_and_emit_bitcode(compile_t* c, const char*** out_files,
  size_t* out_count)
{
  return emit_per_module_bitcode(c, out_files, out_count);
}

void cleanup_bc_files(const char** files, size_t count)
{
  for(size_t i = 0; i < count; i++)
    unlink_file(files[i]);

  ponyint_pool_free_size(count * sizeof(const char*), (void*)files);
}
