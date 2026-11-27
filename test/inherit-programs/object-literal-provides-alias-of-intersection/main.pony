// Make's object literal provides DM, an alias of (Defines & Marker) declared
// after it, and inherits Defines' show. The test expects the literal to get
// show from Defines through the alias and print <6> (#6731).
primitive Make
  fun apply(): Defines val =>
    object is DM end

actor Main
  new create(env: Env) =>
    env.out.print(Make().show(6))

type DM is (Defines & Marker)

trait Defines
  fun show(t: U8): String => "<" + t.string() + ">"

trait Marker
