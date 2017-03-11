primitive PrimitiveInitFinal
  fun _init() =>
    @printf[I32]("primitive init\n".cstring())

  fun _final() =>
    @printf[I32]("primitive final\n".cstring())

class EmbedFinal
  fun _final() =>
    @printf[I32]("embed final\n".cstring())

class ClassFinal
  embed f: EmbedFinal = EmbedFinal

  fun _final() =>
    @printf[I32]("class final\n".cstring())

actor Main
  new create(env: Env) =>
    ClassFinal

  fun _final() =>
    @printf[I32]("actor final\n".cstring())
