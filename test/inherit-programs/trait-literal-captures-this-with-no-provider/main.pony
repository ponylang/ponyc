// T.m() stores this in a local with an inferred type and builds a T literal
// whose name() calls it. No class provides T; the program must compile and
// print nothing: the trait's literal must type check on its own.
trait T
  fun name(): String => "T"
  fun val m(): T val =>
    let me = this
    object val is T
      fun name(): String => "inner of " + me.name()
    end
actor Main
  new create(env: Env) => None
