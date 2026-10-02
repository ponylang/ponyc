use @pony_exitcode[None](code: I32)

class Container[A: Any val]
  var _data: A
  var _code: I32

  new create(data: A) =>
    _data = data
    _code = 0

  new create(data: A) iftype A <: Stringable val =>
    _data = data
    _code = 1

  fun code(): I32 => _code

actor Main
  new create(env: Env) =>
    let c = Container[U32](42)
    @pony_exitcode(c.code())
