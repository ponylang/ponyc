// T[A, B].m returns a literal of T[A, String] whose g uses iftype on B. From C,
// which provides T[U8, Bool], expects "bool"; from that literal, where B is
// String, expects "other".
trait T[A: Any val, B: Any val]
  fun g(): String => "T"
  fun m(): T[A, String] val =>
    object val is T[A, String]
      fun g(): String => iftype B <: Bool then "bool" else "other" end
    end
class C is T[U8, Bool]
actor Main
  new create(env: Env) => env.out.print(C.m().g() + " " + C.m().m().g())
