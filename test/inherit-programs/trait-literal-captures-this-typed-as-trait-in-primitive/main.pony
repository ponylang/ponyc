// T.m binds this to a local typed T val and returns a trait literal that
// captures it. Calling m once on primitive C expects "inner of C".
trait T
  fun name(): String => "T"
  fun val m(): T val =>
    let me: T val = this
    object val is T
      fun name(): String => "inner of " + me.name()
    end
primitive C is T
  fun name(): String => "C"
actor Main
  new create(env: Env) => env.out.print(C.m().name())
