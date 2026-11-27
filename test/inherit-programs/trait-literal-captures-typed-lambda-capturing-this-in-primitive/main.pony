// T.m makes a lambda that captures this, binds it to a typed local, and
// returns a trait literal that captures the lambda. Starting from primitive C,
// expects one more "inner of" at each of three levels.
trait T
  fun name(): String => "T"
  fun val m(): T val =>
    let f: {(): String} val = {()(me = this): String => me.name()} val
    object val is T
      fun name(): String => "inner of " + f()
    end
primitive C is T
  fun name(): String => "C"
actor Main
  new create(env: Env) => env.out.print(C.m().name()); env.out.print(C.m().m().name()); env.out.print(C.m().m().m().name())
