// Twelve levels of traits each return two literals, one with type argument A
// and one with Array[A] val. A class made for every combination would be 2^12
// classes; the test expects the program to compile within the runner's time
// limit and print ok.
trait T12[A: Any val]
  fun z(): String => "z"
trait T11[A: Any val]
  fun a11(): T12[A] val => object val is T12[A] end
  fun b11(): T12[Array[A] val] val => object val is T12[Array[A] val] end
trait T10[A: Any val]
  fun a10(): T11[A] val => object val is T11[A] end
  fun b10(): T11[Array[A] val] val => object val is T11[Array[A] val] end
trait T9[A: Any val]
  fun a9(): T10[A] val => object val is T10[A] end
  fun b9(): T10[Array[A] val] val => object val is T10[Array[A] val] end
trait T8[A: Any val]
  fun a8(): T9[A] val => object val is T9[A] end
  fun b8(): T9[Array[A] val] val => object val is T9[Array[A] val] end
trait T7[A: Any val]
  fun a7(): T8[A] val => object val is T8[A] end
  fun b7(): T8[Array[A] val] val => object val is T8[Array[A] val] end
trait T6[A: Any val]
  fun a6(): T7[A] val => object val is T7[A] end
  fun b6(): T7[Array[A] val] val => object val is T7[Array[A] val] end
trait T5[A: Any val]
  fun a5(): T6[A] val => object val is T6[A] end
  fun b5(): T6[Array[A] val] val => object val is T6[Array[A] val] end
trait T4[A: Any val]
  fun a4(): T5[A] val => object val is T5[A] end
  fun b4(): T5[Array[A] val] val => object val is T5[Array[A] val] end
trait T3[A: Any val]
  fun a3(): T4[A] val => object val is T4[A] end
  fun b3(): T4[Array[A] val] val => object val is T4[Array[A] val] end
trait T2[A: Any val]
  fun a2(): T3[A] val => object val is T3[A] end
  fun b2(): T3[Array[A] val] val => object val is T3[Array[A] val] end
trait T1[A: Any val]
  fun a1(): T2[A] val => object val is T2[A] end
  fun b1(): T2[Array[A] val] val => object val is T2[Array[A] val] end
trait T0[A: Any val]
  fun a0(): T1[A] val => object val is T1[A] end
  fun b0(): T1[Array[A] val] val => object val is T1[Array[A] val] end
primitive K
  fun mk(): T0[U8] val => object val is T0[U8] end
class val C2
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print("ok")
    K.mk().a0(); K.mk().b0()
