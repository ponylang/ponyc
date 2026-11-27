// lib's Foo default calls Helper.name(), and main declares its own Helper. The
// test expects "lib.Helper": the name resolves in lib, where the body is
// written.
use l = "lib"

actor Main
  new create(env: Env) =>
    env.out.print(Bar.make())

primitive Helper
  fun name(): String => "main.Helper (wrong)"

class Bar is l.Foo
