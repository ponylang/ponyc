use @pony_exitcode[None](code: I32)

interface USizeHandler
  fun apply(n: USize): I32

interface StringHandler
  fun apply(s: String): I32

class Overloaded
  fun apply(s: String): I32 => 1
  fun apply(n: USize): I32 => 2

actor Main
  new create(env: Env) =>
    let uh: USizeHandler = Overloaded
    let sh: StringHandler = Overloaded

    let r1 = uh(USize(42))
    let r2 = sh("hello")

    if (r1 == 2) and (r2 == 1) then
      @pony_exitcode(0)
    else
      @pony_exitcode(1)
    end
