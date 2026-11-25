// A trait's default method has default arguments holding an object literal
// with a generic method and a generic lambda, whose type parameters are named
// B. The class that inherits the method has its own type parameter B.
use @pony_exitcode[None](code: I32)

trait T
  fun f(x: U8, o: Any = object fun k[B](b: B): B => b end): U8 => x
  fun g(x: U8, l: Any val = {[B](b: B): B => b} val): U8 => x + 1

class C[B] is T

actor Main
  new create(env: Env) =>
    if C[String].f(3) != 3 then @pony_exitcode(1) end
    if C[String].g(3) != 4 then @pony_exitcode(2) end
