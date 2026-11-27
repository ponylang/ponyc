// #6731's object literal, which provides (Defines & Marker), written in a
// trait default instead of ordinary code. Make inherits apply, and the
// literal's inherited show must print <6>.
trait Mk
  fun apply(): Defines val =>
    object is (Defines & Marker) end
primitive Make is Mk
actor Main
  new create(env: Env) =>
    env.out.print(Make().show(6))
trait Defines
  fun show(t: U8): String => "<" + t.string() + ">"
trait Marker
