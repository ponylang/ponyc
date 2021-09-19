use @pony_exitcode[None](code: I32)

actor Main
  var state: U8 = 0

  new create(env: Env) =>
    msg1(env)
    msg2(env)
    msg3(env)

  be msg1(env: Env) =>
      if state == 0 then state = 1 end

  be msg2(env: Env) =>
    if state == 1 then state = 2 end

  be msg3(env: Env) =>
    if state == 2 then
      @pony_exitcode(I32(1))
    end
