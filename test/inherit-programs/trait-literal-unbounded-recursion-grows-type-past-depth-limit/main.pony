// Each call to m returns a trait literal whose type argument wraps A in one
// more Array, and deep calls m().deep() with no end, so the program needs
// infinitely many types. Expects the instantiation depth error at the trait.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
  fun deep(): String => m().deep()
class C is T[U8]
actor Main
  new create(env: Env) => env.out.print(C.deep())
