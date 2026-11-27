// T.m makes an object literal of T and calls q, declared before m. Main calls
// m() on an object literal of T; the program must print "a".
trait T
  fun q(s: String): String => s
  fun m(): String => (object val is T end).q("a")
actor Main
  new create(env: Env) => env.out.print((object val is T end).m())
