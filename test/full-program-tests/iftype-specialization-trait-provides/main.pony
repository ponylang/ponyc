use @pony_exitcode[None](code: I32)

trait Describable[A: Any val]
  fun describe(): I32 =>
    0

  fun describe(): I32 iftype A <: Stringable val =>
    1

class Container[A: Any val] is Describable[A]
  var _a: A

  new create(a: A) =>
    _a = a

actor Main
  new create(env: Env) =>
    let c = Container[U32](42)
    @pony_exitcode(c.describe())
