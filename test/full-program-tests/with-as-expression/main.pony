use @pony_exitcode[None](code: I32)

class Disposble
  new create() =>
    None

  fun foo() =>
    None

  fun dispose() =>
    None

actor Main
  new create(env: Env) =>
    let exit = with d = Disposble do
      d.foo()
      I32(10)
    end
    @pony_exitcode(exit)

