use m = "main"

use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    if m.Helper() == "from pkg main" then
      @pony_exitcode(1)
    end
