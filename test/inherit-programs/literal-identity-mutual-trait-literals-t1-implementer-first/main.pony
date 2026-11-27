// T1 and T2 return capture-free literals of each other. C provides T1[U8], D
// provides T2[U8], and C is declared first. Expects true twice: a literal
// reached from C or from D is the same object.
trait T1[A: Any val]
  fun n(): String => "T1"
  fun f1(): T2[A] val => object val is T2[A] end
trait T2[A: Any val]
  fun n(): String => "T2"
  fun f2(): T1[A] val => object val is T1[A] end
class val C is T1[U8]
  new val create() => None
class val D is T2[U8]
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print((C.f1() is D.f2().f1()).string() + " " + (D.f2() is C.f1().f2()).string())
