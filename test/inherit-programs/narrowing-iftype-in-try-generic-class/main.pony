// G[String].f: the iftype's then branch indexes a U8 array literal inside a
// try. A is String, so the else branch runs and the program prints 0.
class G[A]
  new create() => None
  fun f(a: A): U8 =>
    try
      iftype A <: U8 then
        [as U8: 1](0)?
      else
        0
      end
    else
      7
    end
actor Main
  new create(env: Env) => env.out.print(G[String].f("x").string())
