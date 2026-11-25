// A generic default method reaches a class through an intermediate trait.
// The intermediate trait and the class both have a type parameter with the
// same name as the method's (`B`), and pass it on as the base trait's type
// argument, so `this` in the inherited copies must keep the inheriting type's
// own `B`.
use @pony_exitcode[None](code: I32)

trait Base[X: Stringable val]
  fun value(): X

  fun describe[B: Stringable val](b: B): String =>
    Describe[X, B](this, b)

primitive Describe[X: Stringable val, B: Stringable val]
  fun apply(base: Base[X] box, b: B): String =>
    b.string() + ":" + base.value().string()

trait Middle[A: Any val, B: Stringable val] is Base[B]

class Impl[A: Any val, B: Stringable val] is Middle[A, B]
  let _b: B

  new create(b: B) =>
    _b = b

  fun value(): B => _b

actor Main
  new create(env: Env) =>
    let impl = Impl[None, String]("abc")
    if impl.describe[U32](42) != "42:abc" then
      @pony_exitcode(1)
    end
