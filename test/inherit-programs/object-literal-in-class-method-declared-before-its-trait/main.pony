// D.f makes an object literal of T and calls T's default q. D is declared
// before T; the program must print "a".
class D
  fun f(): String => (object val is T end).q("a")
trait T
  fun q(s: String): String => s
actor Main
  new create(env: Env) => env.out.print(D.f())
