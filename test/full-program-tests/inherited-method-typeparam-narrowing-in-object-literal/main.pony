// Object literals inherit generic default methods that narrow the method's
// own type parameter, with an iftype in the body or as a guard on a
// specialization, and use the narrowed value.
use @pony_exitcode[None](code: I32)

trait Body
  fun g[A: Any val](a: A): String =>
    iftype A <: Stringable val then
      let s: A = a
      s.string()
    else
      "?"
    end

trait Guard
  fun f[X: Any val](x: X): String => "default"
  fun f[X: Any val](x: X): String iftype X <: Stringable val =>
    "spec " + x.string()

primitive Plain

actor Main
  new create(env: Env) =>
    let b = object val is Body end
    if b.g[U8](2) != "2" then @pony_exitcode(1) end
    if b.g[Plain](Plain) != "?" then @pony_exitcode(2) end
    let g = object val is Guard end
    if g.f[U8](3) != "spec 3" then @pony_exitcode(3) end
    if g.f[Plain](Plain) != "default" then @pony_exitcode(4) end
