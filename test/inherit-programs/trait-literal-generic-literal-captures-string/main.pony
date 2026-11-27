// T[A].m() returns a T[A] literal that captures a String local. Calling n()
// on C.m() and on three levels of m() must print "x x": the local must be
// assigned in every inherited copy of m().
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[A] val =>
    let f: String = "x"
    object val is T[A]
      fun n(): String => f
    end
class val C is T[U8]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(C.m().n() + " " + C.m().m().m().n())
