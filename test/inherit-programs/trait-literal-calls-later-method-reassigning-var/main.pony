// T.m makes a literal of T and calls q, declared after m, which reassigns a
// var local. Expects "ab": a literal in m can call a trait method declared
// later, and the local's type is inferred.
trait T
  fun m(): String => (object val is T end).q()
  fun q(): String =>
    var s = "a"
    s = s + "b"
    s
actor Main
  new create(env: Env) => env.out.print((object val is T end).m())
