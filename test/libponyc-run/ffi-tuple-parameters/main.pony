use "lib:ffi-tuple-parameters-additional"

use @add[I32](arg: (I32, I32))
//use @add_struct[I32](arg: (I32, I32))
use @divmod[(I32, I32)](arg: I32, divisor: I32)
use @pony_exitcode[None](code: I32)

actor Main

  new create(env: Env) =>
    let sum: I32 = @add((1, 2))
    //@add_struct((1, 2))
    (let i: I32, let j: I32) = (2, 1) // @divmod(5, 2)
    @pony_exitcode((sum * 100) + (i * 10) + j) // 321
