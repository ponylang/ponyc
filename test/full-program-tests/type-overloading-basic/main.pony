use @pony_exitcode[None](code: I32)

class Overloaded
  fun apply(s: String): I32 => 1
  fun apply(n: USize): I32 => 2

actor Main
  new create(env: Env) =>
    let o = Overloaded
    let r1 = o("hello")
    let r2 = o(USize(42))

    if (r1 == 1) and (r2 == 2) then
      @pony_exitcode(0)
    else
      @pony_exitcode(1)
    end
