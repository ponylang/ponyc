// A trait default method uses its own type parameter `R` inside a lambda and
// an object literal. The class that inherits it has its own `R`.
use @pony_exitcode[None](code: I32)

interface Box[R: Any val]
  fun get(): R

trait Mapper
  fun lift[R: Any val](r: R): {(): R} val =>
    {(): R => r}

  fun boxed[R: Any val](r: R): Box[R] val =>
    object val is Box[R]
      fun get(): R => r
    end

class Holder[R: Any val] is Mapper

actor Main
  new create(env: Env) =>
    let h = Holder[String]
    if h.lift[U8](3)() != 3 then
      @pony_exitcode(1)
    end
    if h.boxed[U16](4).get() != 4 then
      @pony_exitcode(2)
    end
