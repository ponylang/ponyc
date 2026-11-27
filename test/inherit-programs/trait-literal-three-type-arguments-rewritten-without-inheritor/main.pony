// Each of T's literals sets one of T's three type arguments to U8. Nothing
// provides T; the program must still compile and print x.
trait T[A0: Any val, A1: Any val, A2: Any val]
  fun name(): String => "t"
  fun f0(): T[U8, A1, A2] val => object val is T[U8, A1, A2] end
  fun f1(): T[A0, U8, A2] val => object val is T[A0, U8, A2] end
  fun f2(): T[A0, A1, U8] val => object val is T[A0, A1, U8] end
actor Main
  new create(env: Env) => env.out.print("x")
