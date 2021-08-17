## Fix broken libponyc-standalone.a

When we switched to LLVM 12, we accidentally picked up zlib being required to link against libponyc-standalone.a. That additional dependency makes the standalone library not so standalone.

We've fixed our LLVM configuration so that zlib is no longer needed.
