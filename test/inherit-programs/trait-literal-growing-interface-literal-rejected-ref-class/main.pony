// Interface T[A]'s default m() builds a literal of T[Array[A] val], and Main
// assigns a ref class C to T[U8] structurally. The test expects the body and
// provides-list errors at the interface's literal (issue #1216).
interface T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
class C
  fun n(): String => "C"
  fun m(): T[Array[U8] val] val => object val is T[Array[U8] val] end
actor Main
  new create(env: Env) => let t: T[U8] = C; env.out.print(t.n())
