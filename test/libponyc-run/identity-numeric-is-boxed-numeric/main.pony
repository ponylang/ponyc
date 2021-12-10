use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let boxed: (Any | (U32, U32)) = U32(0)
    if (U32(0) is boxed) and (U32(1) isnt boxed) and
      (U64(0) isnt boxed)
    then
      @pony_exitcode(1)
    end
