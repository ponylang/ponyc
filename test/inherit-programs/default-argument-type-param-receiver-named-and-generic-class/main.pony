// T.run calls a.greet() with no argument, a of type A. C is T[Hi], so its run
// uses Hi's default. G[B] is T[B]; its run sees only Greeter, so G[Hi] uses
// the interface's default.
interface val Greeter
  fun greet(x: String = "iface default"): String

primitive Hi is Greeter
  fun greet(x: String = "Hi default"): String => x

trait T[A: Greeter val]
  fun run(a: A): String => a.greet()

class C is T[Hi]
class G[B: Greeter val] is T[B]

actor Main
  new create(env: Env) =>
    env.out.print("C:    " + C.run(Hi))
    env.out.print("G[Hi]: " + G[Hi].run(Hi))
