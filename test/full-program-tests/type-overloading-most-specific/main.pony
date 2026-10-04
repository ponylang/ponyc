use @pony_exitcode[None](code: I32)

trait Printable
  fun string(): String

class Val is Printable
  fun string(): String => "val"

class Overloaded
  fun apply(s: Printable): USize => 1
  fun apply(s: Val): USize => 2

actor Main
  new create(env: Env) =>
    let o: Overloaded ref = Overloaded
    try
      if o(Val) != 2 then error end
      let p: Printable = Val
      if o(p) != 1 then error end
      @pony_exitcode(0)
    else
      @pony_exitcode(1)
    end
