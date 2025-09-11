use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let counter: USize = 0
    while counter > 0 do
      @pony_exitcode(10)
    else
      @pony_exitcode(20)
    end
