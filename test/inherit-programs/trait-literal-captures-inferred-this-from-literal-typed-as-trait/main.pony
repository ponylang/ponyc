// T.m binds this to an inferred local and returns a trait literal capturing
// it. From an object literal bound to a local typed T val, expects one more
// "inner of" at each of three levels.
trait T
  fun name(): String => "T"
  fun val m(): T val =>
    let me = this
    object val is T
      fun name(): String => "inner of " + me.name()
    end
actor Main
  new create(env: Env) =>
    let t: T val = object val is T
      fun name(): String => "lit"
    end
    env.out.print(t.m().name()); env.out.print(t.m().m().name()); env.out.print(t.m().m().m().name())
