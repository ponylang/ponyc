// T[A].deep() calls deep() on a T[Array[A] val] literal, which would recurse
// through ever larger types. C inherits deep() but nothing calls it, so the
// program must compile and run.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
  fun deep(): String => m().deep()
class C is T[U8]
actor Main
  new create(env: Env) => env.out.print(C.n())
