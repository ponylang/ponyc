// x.greet() omits its argument, with x typed (Hi & A) in r1 and (A & Hi) in
// r2, where A is bounded by Greeter. r1 must use Hi's default and r2
// Greeter's: the program prints Hi default / iface default.
interface val Greeter
  fun greet(x: String = "iface default"): String
primitive Hi is Greeter
  fun greet(x: String = "Hi default"): String => x
class G[A: Greeter val]
  fun r1(x: (Hi & A)): String => x.greet()
  fun r2(x: (A & Hi)): String => x.greet()
actor Main
  new create(env: Env) =>
    env.out.print(G[Hi].r1(Hi) + " / " + G[Hi].r2(Hi))
