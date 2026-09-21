# libponylto

LLVM optimization passes that run during link-time optimization.

The compiler (libponyc) emits per-package bitcode and hands it to lld for linking. lld runs the LTO optimization pipeline, and libponylto's passes run as part of that pipeline. They are a separate library because they execute inside the linker process, not inside the compiler.

lld loads the passes through LLVM's static plugin mechanism: `Extension.def` declares `HANDLE_EXTENSION(PonyLTO)`, which causes lld's pass-builder setup to call `getPonyLTOPluginInfo()` in `plugin.cc`. That function registers each pass at the appropriate pipeline extension point.

## Passes

- **HeapToStack** (`heap_to_stack.cc`): Promotes `pony_alloc` and `pony_alloc_small` calls to stack allocations when the allocated object does not escape the function. Runs as a CGSCC pass so it sees the call graph after cross-module inlining.

- **InlineRemainingAllocPass** (`plugin.cc`): Strips noinline from `pony_alloc` and `pony_alloc_small` and inlines the remaining call sites after HeapToStack has run. Those functions carry noinline (actor.c) so the CGSCC inliner preserves the call sites HeapToStack needs; this pass cleans up afterward. Runs as a Module pass at the optimizer-last extension point.
