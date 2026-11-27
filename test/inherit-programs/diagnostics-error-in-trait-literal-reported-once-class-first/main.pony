// T.m's object literal returns a String where its method says U8. C provides
// T and is declared before it. Expects the error once, at the literal, with no
// note naming C: the literal's code belongs to T, not to C's copy of m.
class C is T
trait T
  fun m(): Any val => object val fun k(): U8 => "x" end
actor Main
  new create(env: Env) => C.m()
