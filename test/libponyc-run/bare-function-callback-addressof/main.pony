use "path:./"
use "lib:bare-function-callback-addressof-additional"

use @baretest_callback[None](cb: Pointer[None], size: USize)
use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    @baretest_callback(addressof foo, 42)

  fun @foo(x: USize) =>
    if x == 42 then
      @pony_exitcode(1)
    end
