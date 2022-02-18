use @pony_exitcode[None](code: I32)
use @printf[I32](fmt: Pointer[U8] tag, ...)

class Disposable
  var _exit_code: I32

  new iso create() =>
    _exit_code = 0

  fun ref set_exit(code: I32) =>
    _exit_code = code

  fun dispose() =>
    @printf("%s\n".cstring(), "hello".cstring())
    @pony_exitcode(_exit_code)

actor Main
  new create(env: Env) =>
    try
      with d = Disposable do
        d.set_exit(10)
        errors()?
      end
    else
      @printf("And the else\n".cstring())
    end

  fun errors() ? =>
    error
