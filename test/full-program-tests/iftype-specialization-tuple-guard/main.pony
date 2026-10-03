use @pony_exitcode[None](code: I32)

class Pair[A: Any val, B: Any val]
  var _first: A
  var _second: B

  new create(first: A, second: B) =>
    _first = first
    _second = second

  fun code(): I32 =>
    0

  fun code(): I32 iftype (A, B) <: (Stringable val, Stringable val) =>
    1

actor Main
  new create(env: Env) =>
    let p = Pair[U32, U32](1, 2)
    @pony_exitcode(p.code())
