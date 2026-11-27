// T.m() builds a lambda that captures this and a T literal whose name() calls
// it. C.m(), C.m().m(), and C.m().m().m() must print one more "inner of " for
// each level: every inherited m() captures the object it was called on.
trait T
  fun name(): String => "T"
  fun val m(): T val =>
    let f = {()(me = this): String => me.name()} val
    object val is T
      fun name(): String => "inner of " + f()
    end
primitive C is T
  fun name(): String => "C"
actor Main
  new create(env: Env) => env.out.print(C.m().name()); env.out.print(C.m().m().name()); env.out.print(C.m().m().m().name())
