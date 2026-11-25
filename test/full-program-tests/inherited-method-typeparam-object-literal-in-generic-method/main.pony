// Object literals inside generic methods get a copy of the method's type
// parameter. Each provides a trait whose default method declares its own type
// parameter with the same name, and the two must stay distinct. `Outer.make`
// and `Wraps.wrap` are trait default methods, inherited by `C` and `Wrapper`,
// so the object literal's type parameter is itself a renamed copy.
use @pony_exitcode[None](code: I32)

trait Inner
  fun pick[A: Stringable val](a: A): String => a.string()

primitive P
  fun make[A: Stringable val](x: A): Inner val =>
    object val is Inner end

trait Outer
  fun make[A: Stringable val](x: A): Inner val =>
    object val is Inner end

class C is Outer

trait Keep[X: Any val]
  fun held(): X
  fun keep[R: Any val](r: R): (X, R) => (held(), r)

primitive Maker
  fun make[R: Any val](r: R): Keep[R] val =>
    object val is Keep[R]
      fun held(): R => r
    end

trait Wraps
  fun wrap[R: Any val](r: R): Keep[R] val =>
    object val is Keep[R]
      fun held(): R => r
    end

class Wrapper is Wraps

actor Main
  new create(env: Env) =>
    if P.make[U8](1).pick[String]("p") != "p" then @pony_exitcode(1) end
    if C.make[U8](1).pick[String]("c") != "c" then @pony_exitcode(2) end
    (let a, let b) = Maker.make[U8](1).keep[String]("x")
    if (a != 1) or (b != "x") then @pony_exitcode(3) end
    (let c, let d) = Wrapper.wrap[U16](2).keep[U32](3)
    if (c != 2) or (d != 3) then @pony_exitcode(4) end
