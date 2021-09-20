use "path:./"
use "lib:additional"

use @ptr_to_int[USize](env: Env)
use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    let dg = digestof env
    if dg == @ptr_to_int(env) then
      @pony_exitcode(1)
    end
