// A trait's default method bodies contain generic methods, a generic
// behaviour, a generic constructor and a generic lambda whose type parameter
// is `X`. The class that inherits the default methods has its own type
// parameter `X`.
use @pony_exitcode[None](code: I32)

trait T
  fun f(): String =>
    let o = object
      fun g[X: Stringable val](x: X): String => x.string()
    end
    o.g[U32](7)

  fun lambda(): String =>
    let l = {[X: Stringable val](x: X): String => x.string()}
    l[U8](8)

  fun behaviour(): String =>
    let o = object
      be g[X: Stringable val](x: X) => None
      fun tag h[X: Stringable val](x: X): String => x.string()
    end
    o.h[U8](9)

  fun constructor(): String =>
    let o = object
      new make[X: Stringable val](x: X) => None
      fun tag h(): String => "10"
    end
    o.h()

class C[X: Stringable val] is T

actor Main
  new create(env: Env) =>
    if C[String].f() != "7" then @pony_exitcode(1) end
    if C[String].lambda() != "8" then @pony_exitcode(2) end
    if C[String].behaviour() != "9" then @pony_exitcode(3) end
    if C[String].constructor() != "10" then @pony_exitcode(4) end
