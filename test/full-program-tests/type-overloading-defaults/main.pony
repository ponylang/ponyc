use @pony_exitcode[None](code: I32)

class Overloaded
  fun apply(a: String, b: String = "default"): USize => 1
  fun apply(a: USize): USize => 2

actor Main
  new create(env: Env) =>
    let o = Overloaded
    try
      if o("hello") != 1 then error end
      if o("hello", "world") != 1 then error end
      if o(USize(42)) != 2 then error end
      @pony_exitcode(0)
    else
      @pony_exitcode(1)
    end
