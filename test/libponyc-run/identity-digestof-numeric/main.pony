use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let value = U32(5)
    let dg = digestof value
    if dg == 5 then
      @pony_exitcode(I32(1))
    end
