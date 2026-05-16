// Regression test for issue #2480.
//
// Partial application of a method whose default argument contains a call
// on a literal (e.g. `USize = -1`, which sugars to `(1).neg()`) used to
// crash the compiler with an internal assertion. The default expression
// was duplicated into the synthesized lambda's apply method, that lambda
// was lifted to an anonymous class whose pass flags got reset, and
// re-running expr on the literal overwrote its coerced USize type with
// TK_LITERAL, leaving the enclosing call's funref inconsistent with its
// receiver.
//
// If the bug ever returns, this test will fail to compile.
//
// Note: nested literal-method-call defaults like `(-1).abs()` still
// crash the compiler with a separate `uifset` assertion. That is a
// related-but-distinct bug tracked as #5342.

primitive _Int2480
  fun apply(x: USize = -1): USize => x

primitive _Float2480
  fun apply(x: F64 = -1.0): F64 => x

primitive _IntBinaryOp2480
  fun apply(x: USize = 1 + 1): USize => x

primitive _FloatBinaryOp2480
  fun apply(x: F64 = 1.0 + 1.0): F64 => x

class _Class2480
  let v: USize
  new create(v': USize = -1) =>
    v = v'

actor Main
  new create(env: Env) =>
    let i = _Int2480~apply()
    let f = _Float2480~apply()
    let bi = _IntBinaryOp2480~apply()
    let bf = _FloatBinaryOp2480~apply()

    // The constructor partial is exercised at compile time. The resulting
    // lambda value is awkward to invoke (val lambda over a ref method),
    // but the partial_application code path is the same one the bug hit
    // and it's what we need to compile cleanly.
    let ctor_partial = _Class2480~create()

    // Build a bitmask so any combination of failures is observable. Using
    // a single shared `env.exitcode` call (last-write-wins) would hide a
    // lower-numbered failure when a higher-numbered one also fires.
    var fails: I32 = 0
    if i() != USize.max_value() then fails = fails or 1 end
    if f() != -1.0 then fails = fails or 2 end
    if bi() != 2 then fails = fails or 4 end
    if bf() != 2.0 then fails = fails or 8 end

    env.exitcode(fails)
