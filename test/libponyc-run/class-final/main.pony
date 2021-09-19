use @pony_exitcode[None](code: I32)

class ClassFinal
  fun _final() =>
    @pony_exitcode(I32(1))

actor Main
  new create(env: Env) =>
    ClassFinal
