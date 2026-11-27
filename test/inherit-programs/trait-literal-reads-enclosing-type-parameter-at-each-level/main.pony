// m's literal provides T[Array[U8] val] and its g reads T's type parameter
// A in an iftype. A is U8 in C's literal and Array[U8] val in every literal
// made from a literal, so the program prints u8 other other.
trait T[A: Any val]
  fun g(): String => "T"
  fun m(): T[Array[U8] val] val =>
    object val is T[Array[U8] val]
      fun g(): String => iftype A <: U8 then "u8" else "other" end
    end
class C is T[U8]
actor Main
  new create(env: Env) => env.out.print(C.m().g() + " " + C.m().m().g() + " " + C.m().m().m().g())
