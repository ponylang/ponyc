## Replace optimize-then-emit pipeline with ThinLTO

The compiler no longer runs the full LLVM optimization pipeline and backend in a single pass over the whole program. It now emits one LLVM module per Pony package, writes each as bitcode, and hands them to lld for per-module optimization and codegen. Peak memory during optimization is proportional to the largest package rather than the whole program.

`--pass asm` and `--pass obj` are removed. The compiler no longer drives the LLVM backend directly — lld does that work during ThinLTO linking. `--pass ir` and `--pass bitcode` still work but now emit earlier-stage IR, after Pony-specific passes and before full optimization.

A new `--pass split-bc` emits the per-package bitcode files without linking. There is no direct replacement for `--pass asm` or `--pass obj` — `--pass bitcode` emits a single bitcode file before splitting, and `--pass split-bc` emits the per-package bitcode files.
