use @printf[I32](fmt: Pointer[None] tag, ...)
use @pony_exitcode[None](code: I32) 

actor Main

  new create(env: Env) =>
    let cb = this~print()
    let chr_num = cb("Hello world!\n".cstring())
    @pony_exitcode(chr_num) // expected 13

  fun @print(fmt: Pointer[None]): I32 =>
    @printf(fmt)
