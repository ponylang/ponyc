// T[B] has f[A: B], and C[Z] provides T[Z], so A's bound is C's parameter Z.
// The test expects C[String].f[String]("ok") to compile and print "ok" (#6729).
trait T[B: Stringable val]
  fun f[A: B](x: A): B => x

class C[Z: Stringable val] is T[Z]

actor Main
  new create(env: Env) =>
    env.out.print(C[String].f[String]("ok"))
