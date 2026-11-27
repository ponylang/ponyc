// A trait default builds an object literal that captures a local, and the
// literal's method has a type error. Two classes provide the trait. The test
// expects the error once, at the literal's text.
trait T
  fun m(): Any val =>
    let v: U8 = 1
    object val fun k(): U8 => v.string() end
class C1 is T
class C2 is T
actor Main
  new create(env: Env) => C1.m(); C2.m()
