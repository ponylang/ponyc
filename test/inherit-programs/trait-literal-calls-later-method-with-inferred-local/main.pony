// T.m makes a literal of T and calls q, declared after m, which binds an
// inferred let local. Expects "a": a literal in m can call a trait method
// declared later, and the local's type is inferred.
trait T
  fun m(): String => (object val is T end).q()
  fun q(): String =>
    let s = "a"
    s
actor Main
  new create(env: Env) => env.out.print((object val is T end).m())
