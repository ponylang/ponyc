// T[A].m returns an object literal providing Stringable whose string() matches
// a U8 against A. Expects "match" for C, where A is U8, and "no" for D, where
// A is String: each class gets its own literal class.
trait T[A: Any val]
  fun m(): Stringable val =>
    object val
      fun string(): String iso^ =>
        let v: Any val = U8(1)
        match v | let a: A => "match".clone() else "no".clone() end
    end
class C is T[U8]
  new create() => None
class D is T[String]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.m().string() + " " + D.m().string())
