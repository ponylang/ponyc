use @pony_exitcode[None](code: I32)

primitive PrimitiveFinal
  fun _final() =>
    @pony_exitcode(1)

actor Main
  new create(env: Env) =>
    None
