use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let a: (U32 | String) = U32(42)
    let b: (U32 | String) = U32(42)
    let c: (U32 | String) = U32(99)

    let same_ok = a is b
    let diff_ok = a isnt c

    let d: (I32 | String) = I32(-1)
    let e: (I32 | String) = I32(-1)
    let f: (I32 | String) = I32(0)

    let signed_same_ok = d is e
    let signed_diff_ok = d isnt f

    if same_ok and diff_ok and signed_same_ok and signed_diff_ok then
      @pony_exitcode(1)
    end
