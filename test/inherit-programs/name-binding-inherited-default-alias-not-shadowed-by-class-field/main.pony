// lib's Foo default calls u.Thing with `use u = "../ua"`, and Bar has a field
// named u. The test expects "ua.Thing": u in the inherited body is lib's
// alias, not Bar's field.
use l = "lib"

actor Main
  new create(env: Env) =>
    env.out.print(Bar.make())

class Bar is l.Foo
  let u: String = "field"
  new create() => None
