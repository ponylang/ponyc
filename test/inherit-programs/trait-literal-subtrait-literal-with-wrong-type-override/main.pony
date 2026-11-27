// T[A]'s default m() builds a literal of subtrait S[Array[A] val]. C provides
// T[U8] and overrides m() to return C, which isn't a T[Array[U8] val]. The
// test expects that one error at C's m() and no errors from T's literal.
trait T[A: Any val]
  fun n(): String => "T"
  fun m(): T[Array[A] val] val => object val is S[Array[A] val] end
trait S[A: Any val] is T[A]
class val C is T[U8]
  new val create() => None
  fun m(): T[Array[U8] val] val => C
actor Main
  new create(env: Env) => env.out.print(C.m().m().n())
