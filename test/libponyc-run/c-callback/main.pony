use "path:./"
use "lib:callback"

use @codegentest_ccallback[I32](self: Callback, cb: Pointer[None], value: I32)
use @pony_exitcode[None](code: I32)

class Callback
  fun apply(value: I32): I32 =>
    value * 2

actor Main
  new create(env: Env) =>
    let cb: Callback = Callback
    let r = @codegentest_ccallback(cb, addressof cb.apply, 3)
    @pony_exitcode(r)
