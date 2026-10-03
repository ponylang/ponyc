use @pony_exitcode[None](code: I32)

class Container[A: Any val]
  var _data: A

  new create(data: A) =>
    _data = data

  fun get(): I32 =>
    0

  fun get(): I32 iftype A <: Stringable val =>
    1

actor Main
  new create(env: Env) =>
    let c = Container[U32](42)
    @pony_exitcode(c.get())
