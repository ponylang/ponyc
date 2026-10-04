use @pony_exitcode[None](code: I32)

class Overloaded
  fun apply(s: String): USize => 1
  fun apply(n: USize): USize => 2

actor Main
  new create(env: Env) =>
    let o = Overloaded
    try
      if o.apply(where s = "hello") != 1 then error end
      if o.apply(where n = USize(42)) != 2 then error end
      @pony_exitcode(0)
    else
      @pony_exitcode(1)
    end
