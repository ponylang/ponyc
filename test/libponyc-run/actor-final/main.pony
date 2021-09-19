use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    None

  fun _final() =>
    @pony_exitcode(I32(1))
