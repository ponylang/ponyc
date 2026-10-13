// Constant integer division and remainder by zero are valid wrapping
// operations. The compiler must not reject them at compile time.

use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let udiv: U64 = 42 / 0
    let urem: U64 = 42 % 0
    let sdiv: I64 = 42 / 0
    let srem: I64 = 42 % 0
    if (udiv == 0) and (urem == 0) and (sdiv == 0) and (srem == 0) then
      @pony_exitcode(1)
    end
