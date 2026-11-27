// T.m makes a lambda that captures this, binds it to an inferred local, and
// returns a trait literal that captures the lambda. Starting from an object
// literal, expects one more "inner of" at each of three levels.
trait T
  fun name(): String => "T"
  fun val m(): T val =>
    let f = {()(me = this): String => me.name()} val
    object val is T
      fun name(): String => "inner of " + f()
    end
actor Main
  new create(env: Env) =>
    let o = object val is T fun name(): String => "O" end
    env.out.print(o.m().m().m().name())
