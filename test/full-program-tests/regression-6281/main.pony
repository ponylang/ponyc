// Regression test for https://github.com/ponylang/ponyc/issues/6281
//
// The constant-folding path in make_divrem extracted 128-bit LLVM constants
// into 64-bit C values via LLVMConstIntGetSExtValue. I128.min_value() has
// all-zero lower 64 bits, so the extraction made I128(0) and I128.min_value()
// compare equal, causing 0 / -1 to return min_value instead of 0.

use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let div_result: I128 = 0 / -1
    let rem_result: I128 = 0 % -1
    if (div_result == I128(0)) and (rem_result == I128(0)) then
      @pony_exitcode(1)
    end
