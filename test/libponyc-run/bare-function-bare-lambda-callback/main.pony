use "path:./"
use "lib:bare-function-bare-lambda-callback"

use @baretest_callback[None](cb: Pointer[None], size: USize)
use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let lbd = @{(x: USize) =>
      if x == 42 then
        @pony_exitcode(1)
      end
    }
    @baretest_callback(lbd, 42)
