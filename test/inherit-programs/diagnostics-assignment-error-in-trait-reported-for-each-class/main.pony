// T's m() assigns a String to a U8, and C1 and C2 both inherit m(). Expects
// "right side must be a subtype of left side" three times: once for T and once
// for each class, with a note naming the inheriting class.
trait T
  fun m(): Any val => let x: U8 = "x"; x
class C1 is T
class C2 is T
actor Main
  new create(env: Env) => C1.m(); C2.m()
