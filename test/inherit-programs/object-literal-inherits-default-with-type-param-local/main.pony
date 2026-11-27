// P.make returns an object literal of Tr[U8], which inherits g. g stores its
// argument in a local of type X. Calling g(7) must print 7.
trait Tr[X: Stringable val]
  fun g(x: X): String =>
    let y: X = x
    y.string()

primitive P
  fun make(): Tr[U8] val => object val is Tr[U8] end

actor Main
  new create(env: Env) =>
    env.out.print(P.make().g(7))
