use "lib:ffi-return-arg-reachable-additional"

use @get_function[@{(U32): U32}](typ: U32)
use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let identity = @get_function(0)
    let add: @{(U32, U32): U32} = @get_function[@{(U32, U32): U32}](1)
    let result = add(identity(1), identity(2))
    @pony_exitcode(result.i32())
