use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let value = (this, env)
    let dg = digestof value
    if dg == ((digestof this) xor (digestof env)) then
      @pony_exitcode(1)
    end
