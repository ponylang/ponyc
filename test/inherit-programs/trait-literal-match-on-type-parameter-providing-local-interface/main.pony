// T[A].m returns an object literal providing interface G whose g() matches a
// U8 against A. Expects "match" for C, where A is U8, and "no" for D, where A
// is String: each class gets its own literal class.
interface val G
  fun g(): String
trait T[A: Any val]
  fun m(): G =>
    object val
      fun g(): String =>
        let v: Any val = U8(1)
        match v | let a: A => "match" else "no" end
    end
class C is T[U8]
  new create() => None
class D is T[String]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.m().g() + " " + D.m().g())
