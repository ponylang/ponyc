// T[A].m() returns one of two T[A] literals, picked by iftype A <: U8. C is
// T[String] and gets "other" from m() and from the literal's own m(); D is
// T[U8] and gets "u8" twice. Each copy of m() must narrow with its own A.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[A] val =>
    iftype A <: U8 then
      object val is T[A] fun n(): String => "u8" end
    else
      object val is T[A] fun n(): String => "other" end
    end
class C is T[String]
class D is T[U8]
actor Main
  new create(env: Env) =>
    env.out.print(C.m().n() + " " + C.m().m().n() + " " + D.m().n() + " " + D.m().m().n())
