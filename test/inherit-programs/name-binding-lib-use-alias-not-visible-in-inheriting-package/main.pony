// lib declares `use u = "../ua"`, and Bar in main inherits a lib default that
// uses it. Main also uses u itself. The test expects "can't find declaration
// of 'u'" at main's use: inheriting the body doesn't bring lib's alias along.
use l = "lib"

actor Main
  new create(env: Env) =>
    env.out.print(Bar.make())
    env.out.print(u.Thing.name() + " (leaked alias)")

class Bar is l.Foo
