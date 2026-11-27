// T.m binds this to a local typed T val and returns a trait literal that
// captures it. Calling m three levels deep from class C expects
// "inner of inner of inner of C".
trait T
  fun name(): String => "T"
  fun val m(): T val =>
    let me: T val = this
    object val is T
      fun name(): String => "inner of " + me.name()
    end
class val C is T
  new val create() => None
  fun name(): String => "C"
actor Main
  new create(env: Env) => env.out.print(C.m().m().m().name())
