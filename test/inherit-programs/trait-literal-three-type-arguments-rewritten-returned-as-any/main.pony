// Each of T's literals sets one of T's three type arguments to U8 and is
// returned as Any val. C provides T, nothing calls the methods, and the
// program must compile and print x.
trait T[A0: Any val, A1: Any val, A2: Any val]
  fun name(): String => "t"
  fun f0(): Any val => object val is T[U8, A1, A2] end
  fun f1(): Any val => object val is T[A0, U8, A2] end
  fun f2(): Any val => object val is T[A0, A1, U8] end
class val C is T[U16, U16, U16]
  new val create() => None
actor Main
  new create(env: Env) =>
    env.out.print("x")
