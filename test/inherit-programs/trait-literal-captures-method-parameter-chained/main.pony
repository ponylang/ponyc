// T.m returns an object literal that captures parameter x and is itself a T.
// Chaining m() three deep must give each literal its own x; expects 123.
trait T
  fun name(): String => "T"
  fun m(x: U8): T val => object val is T fun name(): String => x.string() end
class C is T
actor Main
  new create(env: Env) =>
    env.out.print(C.m(1).name() + C.m(1).m(2).name() + C.m(1).m(2).m(3).name())
