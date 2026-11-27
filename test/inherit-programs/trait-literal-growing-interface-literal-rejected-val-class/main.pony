// Interface I[A]'s default m() builds a literal of I[Array[A] val], and class
// C matches I structurally with its own literal. The test expects the body and
// provides-list errors at the interface's literal (issue #1216).
interface I[A: Any val]
  fun n(): String => "I"
  fun m(): I[Array[A] val] val => object val is I[Array[A] val] end
class val C
  new val create() => None
  fun n(): String => "C"
  fun m(): I[Array[U8] val] val => object val is I[Array[U8] val] end
actor Main
  new create(env: Env) => env.out.print(C.m().m().n())
