// Walk's default lambda calls Walk(x - 1), which uses the same default. The
// call runs only when the lambda does, so this is not a cycle; expects 5.
primitive Walk
  fun apply(n: U8, k: {(U8): U8} val = {(x: U8): U8 => if x == 0 then 0 else 1 + Walk(x - 1) end}): U8 =>
    k(n)
actor Main
  new create(env: Env) => env.out.print(Walk(5).string())
