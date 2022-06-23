use @printf[I32](fmt: Pointer[None] tag, ...)
use @pony_exitcode[None](code: I32) 

actor Main

  new create(env: Env) =>
    let printer = @{(fmt: Pointer[None] tag): I32 => @printf(fmt)}
    let cb = this~print()
    let chr_num1 = printer("Hello".cstring())
    let chr_num2 = cb(" world!\n".cstring())
    @pony_exitcode(chr_num1 + chr_num2) // expected 13

  fun @print(fmt: Pointer[None]): I32 =>
    @printf(fmt)
