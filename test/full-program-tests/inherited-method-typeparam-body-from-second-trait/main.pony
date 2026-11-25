// One trait declares a generic method and another trait provides its body.
// The types that combine them have a type parameter with the same name as the
// method's (`T`): a class, and an object literal inside a generic primitive.
use @pony_exitcode[None](code: I32)

trait Declares
  fun show[T: Stringable val](t: T): String

trait Defines
  fun show[T: Stringable val](t: T): String => "<" + t.string() + ">"

class Both[T: Any val] is (Declares & Defines)

primitive Make[T: Any val]
  fun apply(): Declares val =>
    object is (Declares & Defines) end

actor Main
  new create(env: Env) =>
    if Both[None].show[U8](5) != "<5>" then
      @pony_exitcode(1)
    end
    if Make[None]().show[U16](6) != "<6>" then
      @pony_exitcode(2)
    end
