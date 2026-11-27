// Walker's default lambda calls C.walk(x - 1), where C inherits walk and uses
// the same default. The call runs only when the lambda does, so this is not
// a cycle; expects 5.
trait Walker
  fun walk(n: U8, k: {(U8): U8} val = {(x: U8): U8 => if x == 0 then 0 else 1 + C.walk(x - 1) end}): U8 =>
    k(n)
primitive C is Walker
actor Main
  new create(env: Env) => env.out.print(C.walk(5).string())
