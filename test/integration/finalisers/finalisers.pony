use "../outputcheck"

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
    if env.args.size() > 1
    then
      // we're the child/test process
      ClassFinal
    else
      // we're the parent driver/assert process
      let expected = recover val
        ["primitive init"; "primitive final"; "actor final"; "embed final"; "class final"]
      end
      OutputCheck(expected).run(env)
    end

  fun _final() =>
    @printf[I32]("actor final\n".cstring())
