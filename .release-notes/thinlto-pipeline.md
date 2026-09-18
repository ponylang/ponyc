## Replace optimize-then-emit pipeline with per-package codegen

The compiler now optimizes and generates code one package at a time instead of processing the entire program at once. Peak memory during compilation is proportional to the largest package rather than the whole program. This makes it possible to compile large programs on memory-constrained targets like 32-bit ARM (RPi4) that previously ran out of memory.

A new `--pass split-bc` emits the per-package bitcode files without linking. `--pass ir` and `--pass bitcode` still work but now emit IR after Pony-specific passes and before full optimization.
