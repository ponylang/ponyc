// T.m makes a literal of T and calls q, declared after m, with an argument.
// Expects "a": a literal in m can call a trait method declared later.
trait T
  fun m(): String => (object val is T end).q("a")
  fun q(s: String): String => s
actor Main
  new create(env: Env) => env.out.print((object val is T end).m())
