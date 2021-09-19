use @pony_exitcode[None](code: I32)

actor Main
  new create(env: Env) =>
    foo().bar()

  fun foo(): Main =>
    @pony_exitcode(1)
    this

  fun @bar() =>
    None
