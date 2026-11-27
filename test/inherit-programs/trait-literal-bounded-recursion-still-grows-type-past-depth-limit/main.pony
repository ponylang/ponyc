// T.m returns a literal of T[Array[A] val], and d calls m().d(k - 1). The
// recursion ends at run time, but each level needs a new type, without limit.
// Expects the instantiation depth error at the trait.
trait T[A: Any val]
  fun n(): String => "n"
  fun m(): T[Array[A] val] val => object val is T[Array[A] val] end
  fun d(k: USize): USize => if k == 0 then 0 else m().d(k - 1) + 1 end
primitive K
  fun mk(): T[U8] val => object val is T[U8] end
actor Main
  new create(env: Env) => env.out.print(K.mk().d(3).string())
