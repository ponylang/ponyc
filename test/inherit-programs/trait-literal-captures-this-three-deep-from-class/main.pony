// T.m() stores this in a local with an inferred type and builds a T literal
// whose name() calls it. C.m().m().m().name() must print "inner of inner of
// inner of C": each inherited m() captures the object it was called on.
trait T
  fun name(): String => "T"
  fun val m(): T val =>
    let me = this
    object val is T
      fun name(): String => "inner of " + me.name()
    end
class val C is T
  new val create() => None
  fun name(): String => "C"
actor Main
  new create(env: Env) => env.out.print(C.m().m().m().name())
