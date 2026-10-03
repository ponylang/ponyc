use @pony_exitcode[None](code: I32)

class val NoStringable

class Container[A: Any val]
  var _data: A

  new create(data: A) =>
    _data = data

  fun get(): I32 =>
    1

  fun get(): I32 iftype A <: Stringable val =>
    0

actor Main
  new create(env: Env) =>
    let c = Container[NoStringable](NoStringable)
    @pony_exitcode(c.get())
