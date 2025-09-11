use @pony_exitcode[None](code: I32)

class Disposable
  var _exit_code: I32

  new create() =>
    _exit_code = 0

  fun ref set_exit(code: I32) =>
    _exit_code = code

  fun dispose() =>
    @pony_exitcode(_exit_code)

actor Main
  new create(env: Env) =>
    try
      with d = Disposable do
        d.set_exit(10)
        errors()?
      end
    end

  fun errors() ? =>
    error
