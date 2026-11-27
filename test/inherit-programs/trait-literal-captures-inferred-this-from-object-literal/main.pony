// T.m binds this to an inferred local and returns a trait literal capturing
// it. From an object literal, expects "inner of O" and "inner of inner of O".
trait T
  fun name(): String => "T"
  fun val m(): T val =>
    let me = this
    object val is T
      fun name(): String => "inner of " + me.name()
    end
actor Main
  new create(env: Env) =>
    let o = object val is T fun name(): String => "O" end
    env.out.print(o.m().name()); env.out.print(o.m().m().name())
