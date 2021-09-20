use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let a: (Any, Any) = (U32(0), env)
    let b: (Any, Any) = (U32(1), this)
    if (a is a) and (a isnt b) then
      @pony_exitcode(1)
    end
