// C overrides T.m() and n(). C.p() returns a T literal, and that literal's
// m() is T's, not C's, so C.p().m().n() must print "n".
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
  fun p(): T[Array[A] val] val => object val is T[Array[A] val] end
class val C is T[U8]
  new val create() => None
  fun m(): T[Array[U8] val] val => object val is T[Array[U8] val] fun n(): String => "C" end
  fun n(): String => "C"
actor Main
  new create(env: Env) => env.out.print(C.p().m().n())
