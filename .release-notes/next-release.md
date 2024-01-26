## Fix missing "runtime_info" package documentation

The documentation for the "runtime_info" package wasn't being generated. We've fixed that oversight.

## Use the correct LLVM intrinsics for `powi` on *nix.

We were using outdated LLVM intrinsics `llvm.powi.f32`j and `llvm.powi.f64`; updated to use `llvm.powi.f32.i32` and `llvm.powi.f64.i32`.

