// An object literal in a primitive provides Defines & Marker, both declared
// after the primitive. Expects "<6>": the literal inherits show's default body.
// Issue #6731.
primitive Make
  fun apply(): Defines val =>
    object is (Defines & Marker) end

actor Main
  new create(env: Env) =>
    env.out.print(Make().show(6))

trait Defines
  fun show(t: U8): String => "<" + t.string() + ">"

trait Marker
