// C provides Y, which declares m() with no body, and l.X, whose body calls
// u.Thing with lib's alias. main's u names a different package. The test
// expects "ua.Thing 3" from C.m() and ub.Thing from main's own call.
use l = "lib"
use u = "ub"

trait Y
  fun m[A: Stringable val](a: A): String

class C is (Y & l.X)
  new create() => None

actor Main
  new create(env: Env) =>
    env.out.print(C.m[U8](3))
    env.out.print(u.Thing.name())
