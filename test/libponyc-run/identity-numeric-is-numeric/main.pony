use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    if (U32(0) is U32(0)) and (U32(0) isnt U32(1)) and
      (U32(0) isnt U64(0))
    then
      @pony_exitcode(I32(1))
    end
