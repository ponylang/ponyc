use @pony_exitcode[None](code: I32)

class Container[A: Any val, B: Any val]
  var _a: A
  var _b: B

  new create(a: A, b: B) =>
    _a = a
    _b = b

  fun describe(): I32 =>
    0

  fun describe(): I32 iftype A <: Stringable val and B <: Stringable val =>
    1

actor Main
  new create(env: Env) =>
    let c = Container[U32, U64](42, 99)
    @pony_exitcode(c.describe())
