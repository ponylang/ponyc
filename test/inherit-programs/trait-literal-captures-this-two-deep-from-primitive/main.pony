// T.m() stores this in a local with an inferred type and builds a T literal
// whose name() calls it. C.m() and C.m().m() must print "inner of C" and
// "inner of inner of C": each inherited m() captures the object it was
// called on.
trait T
  fun name(): String => "T"
  fun val m(): T val =>
    let me = this
    object val is T
      fun name(): String => "inner of " + me.name()
    end
primitive C is T
  fun name(): String => "C"
actor Main
  new create(env: Env) => env.out.print(C.m().name()); env.out.print(C.m().m().name())
