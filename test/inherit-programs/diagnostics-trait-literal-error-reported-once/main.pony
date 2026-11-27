// A trait default builds an object literal with no captures whose method has a
// type error, and class C provides the trait. The test expects the error once,
// at the literal's text, with no note about C's inherited copy.
trait T
  fun m(): Any val => object val fun k(): U8 => "x" end
class C is T
actor Main
  new create(env: Env) => C.m()
