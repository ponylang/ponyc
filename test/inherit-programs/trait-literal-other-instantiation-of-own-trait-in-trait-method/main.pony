// T[A].m makes an object literal of T[U8] and calls q(3) on it. Main makes an
// object literal of T[String] and calls m(), so a T[String] copy needs the
// T[U8] literal. The program must print 3.
trait T[A: Stringable val]
  fun q(a: A): String => a.string()
  fun m(): String => (object val is T[U8] end).q(3)
actor Main
  new create(env: Env) => env.out.print((object val is T[String] end).m())
