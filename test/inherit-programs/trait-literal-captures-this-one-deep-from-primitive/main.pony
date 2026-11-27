// T.m() stores this in a local with an inferred type and builds a T literal
// whose name() calls it. C.m().name() must print "inner of C": C's copy of
// m() captures C.
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
  new create(env: Env) => env.out.print(C.m().name())
