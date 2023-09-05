use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    var counter: USize = 5
    while counter > 0 do
      @pony_exitcode(10)
      counter = counter - 1
    else
      @pony_exitcode(20)
    end
