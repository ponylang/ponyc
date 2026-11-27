// T.m binds this to a local typed T val and returns a trait literal that
// captures it. Nothing provides T; the trait and its literal must still
// compile. Expects no output.
trait T
  fun name(): String => "T"
  fun val m(): T val =>
    let me: T val = this
    object val is T
      fun name(): String => "inner of " + me.name()
    end
actor Main
  new create(env: Env) => None
