// lib's Foo default calls Helper.name(), and Bar has a type parameter named
// Helper. The test expects "helper": the name resolves to lib's primitive
// where the body is written, not to Bar's type parameter (#6710).
use l = "lib"

actor Main
  new create(env: Env) =>
    env.out.print(Bar[Wrap].make())

primitive Wrap
  fun name(): String => "WRAP (captured!)"

class Bar[Helper: Wrap] is l.Foo
