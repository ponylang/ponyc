## Replace optimize-then-emit pipeline with ThinLTO

The compiler no longer runs the full LLVM optimization pipeline and backend in a single pass over the whole program. It now splits the LLVM module into partitions, writes each as bitcode, and hands them to lld for per-partition optimization and codegen. Peak memory during optimization is proportional to the largest partition rather than the whole program.

`--pass asm` and `--pass obj` are removed. The compiler no longer drives the LLVM backend directly — lld does that work during ThinLTO linking. `--pass ir` and `--pass bitcode` still work but now emit earlier-stage IR, after Pony-specific passes and before full optimization.

A new `--pass split-bc` emits the partition bitcode files without linking. A new `--thinlto-partitions=N` flag (default 4) controls how many partitions the module is split into. There is no direct replacement for `--pass asm` or `--pass obj` — `--pass bitcode` emits a single bitcode file before splitting, and `--pass split-bc` emits the partition bitcode files.

Debug info (`-d` flag) is not yet supported with ThinLTO builds. Debug builds compile and link correctly but do not emit DWARF info. This will be addressed in a future release.
