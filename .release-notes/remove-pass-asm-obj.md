## Remove `--pass asm` and `--pass obj`

The compiler no longer drives the LLVM backend directly — lld handles optimization and native codegen during ThinLTO linking. `--pass asm` and `--pass obj` have no equivalent in this pipeline.

`--pass bitcode` emits a single bitcode file before per-package splitting. `--pass split-bc` emits the per-package bitcode files that would be handed to lld. `--pass ir` emits textual LLVM IR at the same stage as `--pass bitcode`.
