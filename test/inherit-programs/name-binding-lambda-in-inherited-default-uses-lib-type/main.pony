// A lambda in lib's Foo default calls Helper.name(), and main declares its own
// Helper. The test expects "lib.Helper": names in the lambda resolve in lib.
use l = "lib"

actor Main
  new create(env: Env) =>
    env.out.print(Bar.make())

primitive Helper
  fun name(): String => "main.Helper (wrong)"

class Bar is l.Foo
