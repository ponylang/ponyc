// lib's trait calls name() on lib's private _H both directly and on the value
// mk() returns. main's Bar inherits all three methods. Expects "lib helper"
// twice: inherited bodies are judged in lib, where they are written.
use l = "lib"
class Bar is l.Foo
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(Bar.describe())
    env.out.print(Bar.d2())
