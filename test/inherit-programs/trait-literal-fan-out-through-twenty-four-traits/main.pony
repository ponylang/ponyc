// Each trait's two literals provide the next trait, one with A and one with
// Array[A] val, through 24 traits. The program must compile and print ok.
trait T0[A: Any val]
  fun a0(): Any val => object val is T1[A] end
  fun b0(): Any val => object val is T1[Array[A] val] end
trait T1[A: Any val]
  fun a1(): Any val => object val is T2[A] end
  fun b1(): Any val => object val is T2[Array[A] val] end
trait T2[A: Any val]
  fun a2(): Any val => object val is T3[A] end
  fun b2(): Any val => object val is T3[Array[A] val] end
trait T3[A: Any val]
  fun a3(): Any val => object val is T4[A] end
  fun b3(): Any val => object val is T4[Array[A] val] end
trait T4[A: Any val]
  fun a4(): Any val => object val is T5[A] end
  fun b4(): Any val => object val is T5[Array[A] val] end
trait T5[A: Any val]
  fun a5(): Any val => object val is T6[A] end
  fun b5(): Any val => object val is T6[Array[A] val] end
trait T6[A: Any val]
  fun a6(): Any val => object val is T7[A] end
  fun b6(): Any val => object val is T7[Array[A] val] end
trait T7[A: Any val]
  fun a7(): Any val => object val is T8[A] end
  fun b7(): Any val => object val is T8[Array[A] val] end
trait T8[A: Any val]
  fun a8(): Any val => object val is T9[A] end
  fun b8(): Any val => object val is T9[Array[A] val] end
trait T9[A: Any val]
  fun a9(): Any val => object val is T10[A] end
  fun b9(): Any val => object val is T10[Array[A] val] end
trait T10[A: Any val]
  fun a10(): Any val => object val is T11[A] end
  fun b10(): Any val => object val is T11[Array[A] val] end
trait T11[A: Any val]
  fun a11(): Any val => object val is T12[A] end
  fun b11(): Any val => object val is T12[Array[A] val] end
trait T12[A: Any val]
  fun a12(): Any val => object val is T13[A] end
  fun b12(): Any val => object val is T13[Array[A] val] end
trait T13[A: Any val]
  fun a13(): Any val => object val is T14[A] end
  fun b13(): Any val => object val is T14[Array[A] val] end
trait T14[A: Any val]
  fun a14(): Any val => object val is T15[A] end
  fun b14(): Any val => object val is T15[Array[A] val] end
trait T15[A: Any val]
  fun a15(): Any val => object val is T16[A] end
  fun b15(): Any val => object val is T16[Array[A] val] end
trait T16[A: Any val]
  fun a16(): Any val => object val is T17[A] end
  fun b16(): Any val => object val is T17[Array[A] val] end
trait T17[A: Any val]
  fun a17(): Any val => object val is T18[A] end
  fun b17(): Any val => object val is T18[Array[A] val] end
trait T18[A: Any val]
  fun a18(): Any val => object val is T19[A] end
  fun b18(): Any val => object val is T19[Array[A] val] end
trait T19[A: Any val]
  fun a19(): Any val => object val is T20[A] end
  fun b19(): Any val => object val is T20[Array[A] val] end
trait T20[A: Any val]
  fun a20(): Any val => object val is T21[A] end
  fun b20(): Any val => object val is T21[Array[A] val] end
trait T21[A: Any val]
  fun a21(): Any val => object val is T22[A] end
  fun b21(): Any val => object val is T22[Array[A] val] end
trait T22[A: Any val]
  fun a22(): Any val => object val is T23[A] end
  fun b22(): Any val => object val is T23[Array[A] val] end
trait T23[A: Any val]
  fun a23(): Any val => object val is T24[A] end
  fun b23(): Any val => object val is T24[Array[A] val] end
trait T24[A: Any val]
  fun z(): String => "z"
class val C is T0[U8]
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print("ok")
    C.a0(); C.b0()
