use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let s1: String val = "hello"
    let s2: String val = recover val "world".clone() end

    let a: (U32 | String) = s1
    let b: (U32 | String) = s1
    let c: (U32 | String) = s2

    let same_ok = a is b
    let diff_ok = a isnt c

    let d: (U32 | String) = U32(42)
    let cross_ok = a isnt d

    if same_ok and diff_ok and cross_ok then
      @pony_exitcode(1)
    end
