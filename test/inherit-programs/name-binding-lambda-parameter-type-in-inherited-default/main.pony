// A lambda in lib's Foo default takes a Helper parameter, and main declares
// nothing named Helper. The test expects "lib.Helper": the lambda's parameter
// type resolves in lib.
use l = "lib"

actor Main
  new create(env: Env) =>
    env.out.print(Bar.make())

class Bar is l.Foo
