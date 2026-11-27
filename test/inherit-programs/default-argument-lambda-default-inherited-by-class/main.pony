// T.p's default argument is a lambda. C inherits p and calls it with no
// argument, so the lambda runs and the program prints "x".
trait T
  fun p(f: {(): String} val = {(): String => "x"} val): String => f()
class C is T
actor Main
  new create(env: Env) => env.out.print(C.p())
