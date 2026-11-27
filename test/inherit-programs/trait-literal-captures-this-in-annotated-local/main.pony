// T.m() stores this in a local typed T val and builds a T literal whose
// name() calls it. C.m() and C.m().m() must print "inner of C" and "inner of
// inner of C": the local must be assigned in every inherited copy of m().
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
  new create(env: Env) => env.out.print(C.m().name()); env.out.print(C.m().m().name())
