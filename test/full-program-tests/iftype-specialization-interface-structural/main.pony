use @pony_exitcode[None](code: I32)

interface Describable[A: Any val]
  fun describe(): I32

  fun describe(): I32 iftype A <: Stringable val

class Container[A: Any val] is Describable[A]
  var _a: A

  new create(a: A) =>
    _a = a

  fun describe(): I32 =>
    0

  fun describe(): I32 iftype A <: Stringable val =>
    1

actor Main
  new create(env: Env) =>
    let c: Describable[U32] = Container[U32](42)
    @pony_exitcode(c.describe())
