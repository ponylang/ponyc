// Bar, in main, passes lib's public H to Foo, whose literal calls name() on A
// inside a lambda. Expects "lib.H": a public type argument reaching a nested
// lambda in lib's code compiles and runs.
use l = "lib"
class val Bar is l.Foo[l.H]
  new val create() => None
actor Main
  new create(env: Env) => env.out.print(Bar.g(l.H).show())
