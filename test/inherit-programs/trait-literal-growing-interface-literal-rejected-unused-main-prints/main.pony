// Interface T[A]'s default m() builds a literal of T[Array[A] val], and
// nothing uses T. The test expects the body and provides-list errors at the
// literal (issue #1216).
interface T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
actor Main
  new create(env: Env) => env.out.print("ok")
