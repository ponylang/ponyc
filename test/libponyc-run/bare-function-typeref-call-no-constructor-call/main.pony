use @pony_exitcode[None](code: I32)

class C
  new create() =>
    // This should't be executed
    @pony_exitcode(1)

  fun @foo() =>
    None

actor Main
  new create(env: Env) =>
    C.foo()
