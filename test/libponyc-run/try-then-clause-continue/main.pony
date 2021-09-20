use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    var r: I32 = 0
    while r == 0 do
      try
        if Bool(false) then error else r = 1 end
        continue
      then
        r = 42
      end
    end
    @pony_exitcode(r)
