// T[A].m1() returns a literal that overrides m1() and m2(), the two methods
// of T that build literals. C.m1().m1().n() and C.m2().m1().n() must each
// print "n", so the program prints "nn".
trait T[A: Any val]
  fun n(): String => "n"
  fun m1(): T[Array[A] val] val =>
    object val is T[Array[A] val]
      fun m1(): T[Array[Array[A] val] val] val => error_free()
      fun m2(): T[Array[A] val] val => this
      fun error_free(): T[Array[Array[A] val] val] val => P[Array[Array[A] val] val]
    end
  fun m2(): T[A] val => object val is T[A] end
primitive P[B: Any val] is T[B]
class C is T[U8]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.m1().m1().n() + C.m2().m1().n())
