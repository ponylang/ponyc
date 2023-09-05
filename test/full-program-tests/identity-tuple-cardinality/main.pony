use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    if (env, this, this) isnt (env, this) then
      @pony_exitcode(1)
    end
