// Traits T0 to T15 each have two methods returning object literals of the next
// trait, one with Array[A] and one with (A, U8) as the type argument. K makes
// a T0[U8] literal. The chain is finite, so compiling it must finish, and the
// program must print "ok".
trait T16[A: Any val]
  fun z(): String => "z"
trait T15[A: Any val]
  fun a15(): T16[Array[A] val] val => object val is T16[Array[A] val] end
  fun b15(): T16[(A, U8)] val => object val is T16[(A, U8)] end
trait T14[A: Any val]
  fun a14(): T15[Array[A] val] val => object val is T15[Array[A] val] end
  fun b14(): T15[(A, U8)] val => object val is T15[(A, U8)] end
trait T13[A: Any val]
  fun a13(): T14[Array[A] val] val => object val is T14[Array[A] val] end
  fun b13(): T14[(A, U8)] val => object val is T14[(A, U8)] end
trait T12[A: Any val]
  fun a12(): T13[Array[A] val] val => object val is T13[Array[A] val] end
  fun b12(): T13[(A, U8)] val => object val is T13[(A, U8)] end
trait T11[A: Any val]
  fun a11(): T12[Array[A] val] val => object val is T12[Array[A] val] end
  fun b11(): T12[(A, U8)] val => object val is T12[(A, U8)] end
trait T10[A: Any val]
  fun a10(): T11[Array[A] val] val => object val is T11[Array[A] val] end
  fun b10(): T11[(A, U8)] val => object val is T11[(A, U8)] end
trait T9[A: Any val]
  fun a9(): T10[Array[A] val] val => object val is T10[Array[A] val] end
  fun b9(): T10[(A, U8)] val => object val is T10[(A, U8)] end
trait T8[A: Any val]
  fun a8(): T9[Array[A] val] val => object val is T9[Array[A] val] end
  fun b8(): T9[(A, U8)] val => object val is T9[(A, U8)] end
trait T7[A: Any val]
  fun a7(): T8[Array[A] val] val => object val is T8[Array[A] val] end
  fun b7(): T8[(A, U8)] val => object val is T8[(A, U8)] end
trait T6[A: Any val]
  fun a6(): T7[Array[A] val] val => object val is T7[Array[A] val] end
  fun b6(): T7[(A, U8)] val => object val is T7[(A, U8)] end
trait T5[A: Any val]
  fun a5(): T6[Array[A] val] val => object val is T6[Array[A] val] end
  fun b5(): T6[(A, U8)] val => object val is T6[(A, U8)] end
trait T4[A: Any val]
  fun a4(): T5[Array[A] val] val => object val is T5[Array[A] val] end
  fun b4(): T5[(A, U8)] val => object val is T5[(A, U8)] end
trait T3[A: Any val]
  fun a3(): T4[Array[A] val] val => object val is T4[Array[A] val] end
  fun b3(): T4[(A, U8)] val => object val is T4[(A, U8)] end
trait T2[A: Any val]
  fun a2(): T3[Array[A] val] val => object val is T3[Array[A] val] end
  fun b2(): T3[(A, U8)] val => object val is T3[(A, U8)] end
trait T1[A: Any val]
  fun a1(): T2[Array[A] val] val => object val is T2[Array[A] val] end
  fun b1(): T2[(A, U8)] val => object val is T2[(A, U8)] end
trait T0[A: Any val]
  fun a0(): T1[Array[A] val] val => object val is T1[Array[A] val] end
  fun b0(): T1[(A, U8)] val => object val is T1[(A, U8)] end
primitive K
  fun mk(): T0[U8] val => object val is T0[U8] end
actor Main
  new create(env: Env) =>
    env.out.print("ok")
    K.mk().a0(); K.mk().b0()
