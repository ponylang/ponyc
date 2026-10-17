// No-op stub for the vendored LLVM build.  The real implementation lives in
// src/libponylto/plugin.cc and is linked into the final ponyc binary via
// WHOLE_ARCHIVE; this stub only satisfies the symbol reference so that the
// vendored lld (and any other tool that links LLVMExtensions) can link.
#include <llvm/Plugins/PassPlugin.h>

::llvm::PassPluginLibraryInfo getPonyLTOPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "PonyLTO", "stub",
          [](llvm::PassBuilder &) {}};
}
