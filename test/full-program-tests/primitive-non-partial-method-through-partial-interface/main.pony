// Regression test for partial/non-partial vtable slot segregation on a
// primitive. A primitive's vtable entries are unbox wrappers; when its
// non-partial method is reached through a partial interface, the partial slot
// must forward to a `{T, i1}`-returning wrapper around the unboxed call.
interface box Doubler
  fun apply(x: U32): U32 ?

primitive Twice
  fun apply(x: U32): U32 => x * 2

actor Main
  new create(env: Env) =>
    let d: Doubler = Twice
    let r = try d.apply(21)? else U32(0) end
    if r != 42 then env.exitcode(1) end
