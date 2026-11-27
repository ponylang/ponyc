// T[A].m() builds a T[Array[A] val] literal inside an iftype branch, and no
// class provides T. The program must compile and print "ok": a literal whose
// type argument grows must not make the trait fail to type check.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): String => iftype A <: Stringable val then (object val is T[Array[A] val] end).n() else "other" end
actor Main
  new create(env: Env) => env.out.print("ok")
