use @printf[I32](fmt: Pointer[None] tag, ...)
use @pony_exitcode[None](code: I32) 

actor Main

  new create(env: Env) =>
    let cb = this~foo()
    let sum = cb(Pointer[U8].create(), 1) + cb(Pointer[U32].create(), 1)
    @pony_exitcode(sum) // expected 13

  fun @foo(p: Pointer[None], n: I32): I32 => n
