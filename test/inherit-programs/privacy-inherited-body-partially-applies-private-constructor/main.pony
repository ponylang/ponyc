// lib's trait partially applies the constructor of lib's private class
// _Helper and calls the result. Expects "lib._Helper": the body is written in
// lib, so main's Bar may inherit it and the type of f is inferred normally.
use l = "lib"
class Bar is l.Foo
actor Main
  new create(env: Env) =>
    env.out.print(Bar.make())
