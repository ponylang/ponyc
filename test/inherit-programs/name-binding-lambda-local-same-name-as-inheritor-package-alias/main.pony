// lib's default has a lambda with a local named c, and main declares
// `use c = "collections"`. The test expects "30": the lambda's local c doesn't
// clash with main's alias when Bar inherits the body.
use l = "lib"
use c = "collections"
class Bar is l.Foo
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(Bar.f() + c.Map[String, U8].size().string())
