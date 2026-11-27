// Interface I[A]'s default m() builds a literal of I[Array[A] val], and Main
// assigns a class C to I[None] structurally. The test expects the body and
// provides-list errors at the interface's literal (issue #1216).
interface I[A: Any val]
  fun n(): String => "n"
  fun m(): I[Array[A] val] val => object val is I[Array[A] val] end
class val C
  new val create() => None
  fun n(): String => "C"
  fun m(): I[Array[None] val] val => object val is I[Array[None] val] end
actor Main
  new create(env: Env) =>
    let i: I[None] val = C
    env.out.print(i.m().n())
