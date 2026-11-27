// #6735's object literal written in a trait default. The literal from
// W.make[Y] provides Tr[Y] and inherits g, whose iftype narrows X. With
// Y = U32 and Y = None, the Stringable branch runs and prints 9 and None.
trait Tr[X: Any val]
  fun g(x: X): String =>
    iftype X <: Stringable val then
      x.string()
    else "no" end
trait W
  fun make[Y: Any val](): Tr[Y] val => object is Tr[Y] end
primitive P is W
actor Main
  new create(env: Env) =>
    env.out.print(P.make[U32]().g(9))
    env.out.print(P.make[None]().g(None))
