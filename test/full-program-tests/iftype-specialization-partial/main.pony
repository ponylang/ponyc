use @pony_exitcode[None](code: I32)

class Container[A: Any val]
  var _data: A

  new create(data: A) =>
    _data = data

  fun get(): I32 ? =>
    error

  fun get(): I32 iftype A <: Stringable val ? =>
    if _data.string().size() > 100 then error end
    42

actor Main
  new create(env: Env) =>
    let c = Container[U32](42)
    let r =
      try
        c.get()?
      else
        99
      end
    @pony_exitcode(r)
