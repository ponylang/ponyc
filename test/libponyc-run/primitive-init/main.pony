use @pony_exitcode[None](code: I32)

primitive PrimitiveInit
  fun _init() =>
    @pony_exitcode(I32(1))

actor Main
  new create(env: Env) =>
    None
