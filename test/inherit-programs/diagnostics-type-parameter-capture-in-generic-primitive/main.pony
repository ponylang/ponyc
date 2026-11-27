// The same (A | Foo ref) match as the inherited case, written in a generic
// primitive with no trait. The test expects one capability error at the
// primitive's text, which the inherited case is compared against.
class Foo
  new create() => None
primitive G[A: Any val]
  fun f(x: (A | Foo ref)): String =>
    match x
    | let y: A => "a"
    else "other" end
actor Main
  new create(env: Env) =>
    env.out.print(G[Foo val].f(Foo))
    env.out.print(G[Foo val].f(recover val Foo end))
