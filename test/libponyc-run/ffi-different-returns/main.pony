use "lib:ffi-different-returns-additional"

use @ffi_function[I32]()
use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let res: (I32 | U8) = @ffi_function[U8]()
    match res
    | let _: U8 => @pony_exitcode(1)
    | let _: I32 => @pony_exitcode(2)
    end
