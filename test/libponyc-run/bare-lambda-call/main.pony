use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let lbd = @{(x: USize) =>
      if x == 42 then
        @pony_exitcode(1)
      end
    }
    lbd(42)
