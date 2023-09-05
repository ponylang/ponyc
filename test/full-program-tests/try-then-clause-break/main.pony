use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    var r: I32 = 0
    while true do
      try
        if Bool(false) then error end
        break
      then
        r = 42
      end
    end
    @pony_exitcode(r)
