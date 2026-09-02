use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    // Dispatch .string() through Stringable on a tagged U32
    let s: Stringable = U32(42)
    let result = s.string()

    // Dispatch .string() through Stringable on a tagged Bool
    let s2: Stringable = true
    let result2 = s2.string()

    if (result == "42") and (result2 == "true") then
      @pony_exitcode(1)
    end
