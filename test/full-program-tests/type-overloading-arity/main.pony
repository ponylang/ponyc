use @pony_exitcode[None](code: I32)

class Greeter
  fun apply(): String => "hello"
  fun apply(name: String): String => "hello, " + name

actor Main
  new create(env: Env) =>
    let g = Greeter
    let r1 = g()
    let r2 = g("world")

    if (r1 == "hello") and (r2 == "hello, world") then
      @pony_exitcode(0)
    else
      @pony_exitcode(1)
    end
