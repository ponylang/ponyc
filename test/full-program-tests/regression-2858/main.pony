// Regression test for https://github.com/ponylang/ponyc/issues/2858
//
// Wrapping division of a signed integer's minimum value by -1 must produce the
// minimum value, not zero. The mathematical result overflows to MIN_VALUE, the
// same way MAX_VALUE + 1 wraps to MIN_VALUE.
//
// The literals below are compile-time constants, so this also verifies that the
// compiler accepts the expression rather than rejecting it as a constant
// overflow error.

use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let div_result: I8 = -128 / -1
    let rem_result: I8 = -128 % -1
    if (div_result == I8.min_value()) and (rem_result == I8(0)) then
      @pony_exitcode(1)
    end
