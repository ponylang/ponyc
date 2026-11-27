// T1 and T2 return capture-free literals of each other, and only C provides
// T1[U8]. Expects true twice: each literal is the same object at the first
// level and two levels deeper.
trait T1[A: Any val]
  fun n(): String => "T1"
  fun f1(): T2[A] val => object val is T2[A] end
trait T2[A: Any val]
  fun n(): String => "T2"
  fun f2(): T1[A] val => object val is T1[A] end
class val C is T1[U8]
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print((C.f1() is C.f1().f2().f1()).string() + " " + (C.f1().f2() is C.f1().f2().f1().f2()).string())
