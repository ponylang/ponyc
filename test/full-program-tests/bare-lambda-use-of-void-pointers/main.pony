use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let cb = @{(p: Pointer[None], n: I32): I32 => n}
    let sum = cb(Pointer[U8].create(), 1) + cb(Pointer[U32].create(), 1)
    @pony_exitcode(sum) // should be 2
