// Regression test for https://github.com/ponylang/ponyc/issues/6423
//
// LLVMConstIntGetSExtValue returned only the low 64 bits of a 128-bit divisor
// passed to make_divrem. A divisor whose low 64 bits are all ones
// (e.g. 0xFFFF_FFFF_FFFF_FFFF, which is 2^64-1 not -1) truncated to -1, and
// the signed-min-divided-by-negative-one special case ran incorrectly.

use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let d: I128 = -0x8000_0000_0000_0000_0000_0000_0000_0000 / 0xFFFF_FFFF_FFFF_FFFF
    let r: I128 = -0x8000_0000_0000_0000_0000_0000_0000_0000 % 0xFFFF_FFFF_FFFF_FFFF

    // I128.min_value() / (2^64-1) truncates to -2^63
    // I128.min_value() % (2^64-1) is -2^63
    let expected_div: I128 = -9223372036854775808
    let expected_rem: I128 = -9223372036854775808

    if (d == expected_div) and (r == expected_rem) then
      @pony_exitcode(1)
    end
