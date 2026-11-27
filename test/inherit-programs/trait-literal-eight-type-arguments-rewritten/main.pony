// Each of T's literals sets one of T's eight type arguments to U8, so
// chained calls reach a bounded set of argument combinations. The program
// must compile and print t.
trait T[A0: Any val, A1: Any val, A2: Any val, A3: Any val, A4: Any val, A5: Any val, A6: Any val, A7: Any val]
  fun name(): String => "t"
  fun f0(): T[U8, A1, A2, A3, A4, A5, A6, A7] val => object val is T[U8, A1, A2, A3, A4, A5, A6, A7] end
  fun f1(): T[A0, U8, A2, A3, A4, A5, A6, A7] val => object val is T[A0, U8, A2, A3, A4, A5, A6, A7] end
  fun f2(): T[A0, A1, U8, A3, A4, A5, A6, A7] val => object val is T[A0, A1, U8, A3, A4, A5, A6, A7] end
  fun f3(): T[A0, A1, A2, U8, A4, A5, A6, A7] val => object val is T[A0, A1, A2, U8, A4, A5, A6, A7] end
  fun f4(): T[A0, A1, A2, A3, U8, A5, A6, A7] val => object val is T[A0, A1, A2, A3, U8, A5, A6, A7] end
  fun f5(): T[A0, A1, A2, A3, A4, U8, A6, A7] val => object val is T[A0, A1, A2, A3, A4, U8, A6, A7] end
  fun f6(): T[A0, A1, A2, A3, A4, A5, U8, A7] val => object val is T[A0, A1, A2, A3, A4, A5, U8, A7] end
  fun f7(): T[A0, A1, A2, A3, A4, A5, A6, U8] val => object val is T[A0, A1, A2, A3, A4, A5, A6, U8] end
class val C is T[U16, U16, U16, U16, U16, U16, U16, U16]
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print(C.f0().f1().name())
