use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    foo(42)

  fun @foo(x: USize) =>
    if x == 42 then
      @pony_exitcode(1)
    end
