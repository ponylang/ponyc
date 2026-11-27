// lib and main each declare `use u` for the same ua package, and Bar inherits
// lib's default that calls u.Thing. The test expects ua.Thing twice: each
// package's alias resolves in that package.
use l = "lib"
use u = "ua"

actor Main
  new create(env: Env) =>
    env.out.print(Bar.make())
    env.out.print(u.Thing.name() + "")

class Bar is l.Foo
