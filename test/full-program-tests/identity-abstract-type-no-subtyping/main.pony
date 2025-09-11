use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let a: (Env | None) = None
    let b: (Main | None) = None
    if a is b then
      @pony_exitcode(1)
    end
