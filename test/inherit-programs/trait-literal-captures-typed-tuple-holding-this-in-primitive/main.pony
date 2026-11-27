// T.m binds a tuple holding this to a typed local and returns a trait literal
// that captures it. Expects "inner of C" and "inner of inner of C": the capture
// works when m runs on primitive C and on the literal m returned.
trait T
  fun name(): String => "T"
  fun val m(): T val =>
    let pair: (T val, U8) = (this, U8(1))
    object val is T
      fun name(): String => "inner of " + pair._1.name()
    end
primitive C is T
  fun name(): String => "C"
actor Main
  new create(env: Env) => env.out.print(C.m().name()); env.out.print(C.m().m().name())
