// T's m() assigns a String to a U8 next to an object literal, and C inherits
// m(). Expects "right side must be a subtype of left side" twice: once for T
// and once for C's copy, with a note naming method 'm' inherited by C from T.
trait T
  fun m(): Any val =>
    let v: U8 = 1
    let w: U8 = "x"
    object val fun k(): U8 => v end
class C is T
actor Main
  new create(env: Env) => C.m()
