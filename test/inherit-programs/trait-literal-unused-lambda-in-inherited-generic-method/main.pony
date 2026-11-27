// T[A]'s generic default f[B] builds a lambda over A that it never calls, and
// C provides T[String]. The test expects "h1": the inherited method compiles
// and runs with the unused lambda in it.
actor Main
  new create(env: Env) => env.out.print(C.f[U8](1))

trait T[A: Stringable val]
  fun f[B: Stringable val](b: B): String =>
    let h: Helper = Helper
    let l = {(x: A): String => x.string()}
    h.name() + b.string()

primitive Helper
  fun name(): String => "h"

class C is T[String]
