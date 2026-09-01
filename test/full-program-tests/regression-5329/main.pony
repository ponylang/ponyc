use @pony_exitcode[None](code: I32)

// Regression test for https://github.com/ponylang/ponyc/issues/5329.
//
// The compiler's float-literal lexer used a naive
// significand * pow(10, exponent) conversion that was not correctly rounded.
// 2.2250738585072014e-308 (the smallest normal F64) parsed to zero because
// pow(10, -324) underflowed inside the evaluator. The lexer now calls
// strtod, which is correctly rounded per IEEE 754.

actor Main
  new create(env: Env) =>
    let a: F64 = 2.2250738585072014e-308
    let ok = a.bits() == 0x0010000000000000
    @pony_exitcode(I32(if ok then 1 else 0 end))
