// A trait's generic default method f calls the trait's generic method g, and
// both name their type parameter R. f calls g directly and through partial
// application, each time once with a concrete type argument and once with its
// own R. An object literal in a generic method and a class, each with a type
// parameter R of its own, inherit both methods.
use @pony_exitcode[None](code: I32)

trait T
  fun tag g[R: Stringable val](r: R): String => r.string()
  fun tag f[R: Stringable val](r: R): String =>
    let h = this~g[U8]()
    let k = this~g[R]()
    h(1) + k(r) + this.g[U16](2) + g[R](r)

primitive Maker
  fun make[R: Any val](): T val =>
    object val is T end

class C[R: Any val] is T

actor Main
  new create(env: Env) =>
    if Maker.make[String]().f[U32](7) != "1727" then @pony_exitcode(1) end
    if C[String].f[U32](8) != "1828" then @pony_exitcode(2) end
