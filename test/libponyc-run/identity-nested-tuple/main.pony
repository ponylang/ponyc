use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let a: (((U8, U16) | None, U8) | None) = None
    let b: (((U8, U16) | None, U8) | None) = None
    if a is b then
      @pony_exitcode(1)
    end
