// Each trait's two literals provide the next trait with A wrapped in a
// tuple, through twelve traits. The program must compile and print t2.
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
  fun fa(): T5[(A, U8)] val => object val is T5[(A, U8)] end
  fun fb(): T5[(A, U16)] val => object val is T5[(A, U16)] end
trait T5[A: Any val]
  fun name(): String => "t5"
  fun fa(): T6[(A, U8)] val => object val is T6[(A, U8)] end
  fun fb(): T6[(A, U16)] val => object val is T6[(A, U16)] end
trait T6[A: Any val]
  fun name(): String => "t6"
  fun fa(): T7[(A, U8)] val => object val is T7[(A, U8)] end
  fun fb(): T7[(A, U16)] val => object val is T7[(A, U16)] end
trait T7[A: Any val]
  fun name(): String => "t7"
  fun fa(): T8[(A, U8)] val => object val is T8[(A, U8)] end
  fun fb(): T8[(A, U16)] val => object val is T8[(A, U16)] end
trait T8[A: Any val]
  fun name(): String => "t8"
  fun fa(): T9[(A, U8)] val => object val is T9[(A, U8)] end
  fun fb(): T9[(A, U16)] val => object val is T9[(A, U16)] end
trait T9[A: Any val]
  fun name(): String => "t9"
  fun fa(): T10[(A, U8)] val => object val is T10[(A, U8)] end
  fun fb(): T10[(A, U16)] val => object val is T10[(A, U16)] end
trait T10[A: Any val]
  fun name(): String => "t10"
  fun fa(): T11[(A, U8)] val => object val is T11[(A, U8)] end
  fun fb(): T11[(A, U16)] val => object val is T11[(A, U16)] end
trait T11[A: Any val]
  fun name(): String => "t11"
  fun fa(): T12[(A, U8)] val => object val is T12[(A, U8)] end
  fun fb(): T12[(A, U16)] val => object val is T12[(A, U16)] end
trait T12[A: Any val]
  fun name(): String => "t12"
class val C is T0[U32]
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.fa().fb().name())
