use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let a: ((Any, U8) | None) = (U8(0), 0)
    let b: ((U8, U8) | None) = (0, 0)
    if a is b then
      @pony_exitcode(I32(1))
    end
