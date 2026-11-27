// Interface I[A]'s default m() builds a literal of I[Array[A] val]. G[A]
// overrides m() with a named class. The test expects the body and
// provides-list errors at the interface's own literal (issue #1216).
interface I[A: Any val]
  fun n(): String => "n"
  fun m(): I[Array[A] val] val => object val is I[Array[A] val] end
class val G[A: Any val] is I[A]
  new val create() => None
  fun m(): I[Array[A] val] val => G[Array[A] val]
  fun n(): String => "G"
actor Main
  new create(env: Env) => env.out.print(G[U8].m().m().n())
