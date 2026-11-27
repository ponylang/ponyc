// T[A].m returns an object literal of S[Array[A]], where S is T, so the
// recursion goes through a subtrait. No type provides T or S; the program
// must compile and print nothing.
trait T[A: Any val]
  fun n(): String => "T"
  fun m(): T[Array[A] val] val => object val is S[Array[A] val] end
trait S[A: Any val] is T[A]
actor Main
  new create(env: Env) => None
