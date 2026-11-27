// The generic method P.make[Y] returns an object literal of Tr[Y], which
// inherits g. g stores its argument in a local of type X. P.make[U32]() then
// g(5) must print 5.
actor Main
  new create(env: Env) =>
    env.out.print(P.make[U32]().g(5).string())

trait Tr[X: Stringable val]
  fun g(x: X): X =>
    let y: X = x
    y

primitive P
  fun make[Y: Stringable val](): Tr[Y] val => object val is Tr[Y] end
