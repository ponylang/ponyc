use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let a: (U32 | String) = U32(42)
    let b: (U32 | String) = U32(42)
    let c: (U32 | String) = U32(99)

    let da = digestof a
    let db = digestof b
    let dc = digestof c

    let same_ok = da == db
    let diff_ok = da != dc

    if same_ok and diff_ok then
      @pony_exitcode(1)
    end
