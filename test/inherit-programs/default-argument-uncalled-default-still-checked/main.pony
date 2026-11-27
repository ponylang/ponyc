// C.g's default argument is a try that can never error, and nothing calls g.
// The default must still be checked, so the program fails with "try
// expression never results in an error".
class C
  new create() => None
  fun g(x: U8 = try 3 else 4 end): U8 => x
actor Main
  new create(env: Env) => env.out.print("x")
