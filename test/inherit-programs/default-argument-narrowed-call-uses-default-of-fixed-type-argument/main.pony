// T.run calls a.greet() with no argument inside an iftype on (A, B). G2 fixes
// A to Hi, so its call uses Hi's default and prints "Hi default". Gen leaves
// A generic, so its call uses the default declared on the Greeter constraint
// and prints "iface default", even when Gen is instantiated with Hi.
interface val Greeter
  fun greet(x: String = "iface default"): String
primitive Hi is Greeter
  fun greet(x: String = "Hi default"): String => x
trait T[A: Greeter val, B: Any val]
  fun run(a: A): String =>
    iftype (A, B) <: (Hi, U8) then a.greet() else "-" end
class G2[Bx: Any val] is T[Hi, Bx]
  new create() => None
class Gen[Ax: Greeter val, Bx: Any val] is T[Ax, Bx]
  new create() => None
actor Main
  new create(env: Env) =>
    env.out.print(G2[U8].run(Hi))
    env.out.print(G2[String].run(Hi))
    env.out.print(Gen[Hi, U8].run(Hi))
