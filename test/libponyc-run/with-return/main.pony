use @pony_exitcode[None](code: I32)

class Disposble
  var _exit_code: I32

  new create() =>
    _exit_code = 0

  fun ref set_exit(code: I32) =>
    ifdef not debug then
      _exit_code = code
    end

  fun dispose() =>
    @pony_exitcode(_exit_code)

actor Main
  new create(env: Env) =>
    with d = Disposble do
      d.set_exit(10)
      return
    end
