use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    try
      try
        if false then error end
        return
      end
    then
      @pony_exitcode(42)
    end
    @pony_exitcode(0)
