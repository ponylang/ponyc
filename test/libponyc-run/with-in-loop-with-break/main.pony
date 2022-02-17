use @pony_exitcode[None](code: I32)

class Disposble
  var _exit_code: I32

  new create() =>
    _exit_code = 0

  fun ref set_exit(code: I32) =>
    _exit_code = code

  fun dispose() =>
    @pony_exitcode(_exit_code)

actor Main
  new create(env: Env) =>
    var counter: I32 = 0
    while true do
      counter = counter + 1
      with d = Disposble do
        d.set_exit(counter)
        if counter == 10 then
          break
        end
      end
    end
