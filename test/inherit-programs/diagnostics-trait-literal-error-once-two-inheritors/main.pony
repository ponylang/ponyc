// A trait default builds an object literal with no captures whose method has a
// type error, and two classes provide the trait. The test expects the error
// once, at the literal's text.
trait T
  fun m(): Any val => object val fun k(): U8 => "x" end
class C1 is T
class C2 is T
actor Main
  new create(env: Env) => C1.m(); C2.m()
