// A generic class with the same recursion as the trait literal tests, with no
// trait and no literal: P[A].m returns P[Array[A]]. Calling m() three times
// and then n() must compile and print "n".
class val P[A: Any val]
  new val create() => None
  fun m(): P[Array[A] val] val => P[Array[A] val]
  fun n(): String => "n"
actor Main
  new create(env: Env) => env.out.print(P[U8].m().m().m().n())
