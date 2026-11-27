// Interface I[A]'s default m() builds a literal of I[Array[A] val], so each
// literal's type needs a literal of a larger type. Nothing uses I. The test
// expects the body and provides-list errors at the literal (issue #1216).
interface I[A: Any val]
  fun n(): String => "I"
  fun m(): I[Array[A] val] val => object val is I[Array[A] val] end
actor Main
  new create(env: Env) => None
