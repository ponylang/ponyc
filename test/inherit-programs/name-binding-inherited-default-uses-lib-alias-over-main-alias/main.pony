// lib's Foo default calls u.Thing with `use u = "../ua"`, and main declares
// `use u = "ub"`. The test expects ua.Thing from the inherited body and
// ub.Thing from main's own call.
use l = "lib"
use u = "ub"

actor Main
  new create(env: Env) =>
    env.out.print(Bar.make())
    env.out.print(u.Thing.name())

class Bar is l.Foo
