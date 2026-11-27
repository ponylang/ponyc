// T.m binds a tuple holding this to an inferred local and returns a trait
// literal that captures it. Starting from an object literal, expects
// "inner of inner of O".
trait T
  fun name(): String => "T"
  fun val m(): T val =>
    let pair = (this, U8(1))
    object val is T
      fun name(): String => "inner of " + pair._1.name()
    end
actor Main
  new create(env: Env) =>
    let o = object val is T fun name(): String => "O" end
    env.out.print(o.m().m().name())
