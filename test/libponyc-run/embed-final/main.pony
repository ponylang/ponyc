use @pony_exitcode[None](code: I32)

class EmbedFinal
  fun _final() =>
    @pony_exitcode(1)

actor Main
  embed c: EmbedFinal = EmbedFinal

  new create(env: Env) =>
    None
