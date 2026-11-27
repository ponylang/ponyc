// C.m returns an object literal that captures C and whose own m returns this.
// Calling name() on C.m() and on C.m().m() must both print "inner of C".
trait T
  fun name(): String => "T"
  fun val m(): T val
primitive C is T
  fun name(): String => "C"
  fun val m(): T val =>
    let me = this
    object val is T
      fun name(): String => "inner of " + me.name()
      fun val m(): T val => this
    end
actor Main
  new create(env: Env) => env.out.print(C.m().name()); env.out.print(C.m().m().name())
