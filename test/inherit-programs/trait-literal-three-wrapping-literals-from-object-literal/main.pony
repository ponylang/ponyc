// T's m1, m2, m3 return literals wrapping A in W1, W2, W3. Starting from an
// object literal of T[U8], expects "nnn": every wrapped literal compiles.
class val W1[X: Any val]
class val W2[X: Any val]
class val W3[X: Any val]
trait T[A: Any val]
  fun n(): String => "n"
  fun m1(): T[W1[A]] val => object val is T[W1[A]] end
  fun m2(): T[W2[A]] val => object val is T[W2[A]] end
  fun m3(): T[W3[A]] val => object val is T[W3[A]] end
actor Main
  new create(env: Env) => let o = object val is T[U8] end
    env.out.print(o.n() + o.m1().n() + o.m2().m3().n())
