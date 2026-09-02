use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let x: Any val = U32(42)

    let matched =
      match x
      | let u: U32 => u == 42
      else
        false
      end

    if matched then
      @pony_exitcode(1)
    end
