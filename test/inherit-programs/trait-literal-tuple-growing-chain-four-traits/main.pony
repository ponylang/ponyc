// Each trait's two literals provide the next trait with A wrapped in a
// tuple, through four traits. The program must compile and print t2.
trait T0[A: Any val]
  fun name(): String => "t0"
  fun fa(): T1[(A, U8)] val => object val is T1[(A, U8)] end
  fun fb(): T1[(A, U16)] val => object val is T1[(A, U16)] end
trait T1[A: Any val]
  fun name(): String => "t1"
  fun fa(): T2[(A, U8)] val => object val is T2[(A, U8)] end
  fun fb(): T2[(A, U16)] val => object val is T2[(A, U16)] end
trait T2[A: Any val]
  fun name(): String => "t2"
  fun fa(): T3[(A, U8)] val => object val is T3[(A, U8)] end
  fun fb(): T3[(A, U16)] val => object val is T3[(A, U16)] end
trait T3[A: Any val]
  fun name(): String => "t3"
  fun fa(): T4[(A, U8)] val => object val is T4[(A, U8)] end
  fun fb(): T4[(A, U16)] val => object val is T4[(A, U16)] end
trait T4[A: Any val]
  fun name(): String => "t4"
class val C is T0[U32]
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.fa().fb().name())
