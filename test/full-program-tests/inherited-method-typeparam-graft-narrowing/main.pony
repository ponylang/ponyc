// A method's signature comes from one trait and its body from another, and
// the body narrows one of the method's type parameters with an iftype and
// uses a method only the narrowed type has. Both a class and an object literal
// inherit `show`, and a second class names its own type parameter T like the
// method's; `pair` narrows its second type parameter.
use @pony_exitcode[None](code: I32)

trait Declares[X: Stringable val]
  fun show[T: Stringable val](t: T, x: X): String

trait Defines[Y: Stringable val]
  fun show[T: Stringable val](t: T, x: Y): String =>
    iftype T <: U8 then
      let u: T = t
      (u + 1).string() + x.string()
    else
      t.string() + x.string()
    end

trait DeclaresPair
  fun pair[S: Stringable val, T: Stringable val](s: S, t: T): String

trait DefinesPair
  fun pair[S: Stringable val, T: Stringable val](s: S, t: T): String =>
    iftype T <: U8 then
      let u: T = t
      s.string() + (u + 1).string()
    else
      s.string() + t.string()
    end

class C[Z: Stringable val] is
  (Declares[Z] & Defines[Z] & DeclaresPair & DefinesPair)

class D[T: Stringable val] is (Declares[T] & Defines[T])

primitive Make[Z: Stringable val]
  fun apply(): Declares[Z] val =>
    object val is (Declares[Z] & Defines[Z]) end

actor Main
  new create(env: Env) =>
    if C[String].show[U8](5, "a") != "6a" then @pony_exitcode(1) end
    if C[String].show[U16](5, "b") != "5b" then @pony_exitcode(2) end
    if Make[String]().show[U8](7, "c") != "8c" then @pony_exitcode(3) end
    if Make[String]().show[U16](7, "d") != "7d" then @pony_exitcode(4) end
    if C[String].pair[String, U8]("s", 5) != "s6" then @pony_exitcode(5) end
    if C[String].pair[U8, U16](1, 5) != "15" then @pony_exitcode(6) end
    if D[String].show[U8](5, "e") != "6e" then @pony_exitcode(7) end
    if D[String].show[U16](5, "f") != "5f" then @pony_exitcode(8) end
