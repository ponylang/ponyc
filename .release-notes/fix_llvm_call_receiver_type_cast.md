## Fix call receiver type casting in LLVM IR

In some rare cases (such as in the PonyCheck library), `ponyc` was generating LLVM that did not use the correct pointer types for call site receiver arguments. This did not result in unsafe code at runtime, but it was still a violation of the LLVM type system, and it caused the "Verification" step of compilation to fail when enabled.

To fix this issue, we introduced an LLVM bitcast at such call sites to cast the receiver pointer to the correct LLVM type.
