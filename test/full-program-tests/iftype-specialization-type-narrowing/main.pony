use @pony_exitcode[None](code: I32)

class Container[A: Any val]
  var _data: A

  new create(data: A) =>
    _data = data

  fun get(): I32 =>
    0

  fun get(): I32 iftype A <: Stringable val =>
    if _data.string().size() > 0 then 1 else 0 end

actor Main
  new create(env: Env) =>
    let c = Container[U32](99)
    @pony_exitcode(c.get())
