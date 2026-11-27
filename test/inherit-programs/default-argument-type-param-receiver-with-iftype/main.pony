// T.plain calls a.greet() with no argument; T.narrowed makes the same call
// inside iftype A <: Hi. C is T[Hi] and uses Hi's default in both. G[B] is
// T[B], and G[Hi] uses Greeter's default in both, narrowed or not.
interface val Greeter
  fun greet(x: String = "iface default"): String

primitive Hi is Greeter
  fun greet(x: String = "Hi default"): String => x

trait T[A: Greeter val]
  fun plain(a: A): String => a.greet()
  fun narrowed(a: A): String =>
    iftype A <: Hi then a.greet() else "-" end

class C is T[Hi]
class G[B: Greeter val] is T[B]

actor Main
  new create(env: Env) =>
    env.out.print("C plain:     " + C.plain(Hi))
    env.out.print("C narrowed:  " + C.narrowed(Hi))
    env.out.print("G plain:     " + G[Hi].plain(Hi))
    env.out.print("G narrowed:  " + G[Hi].narrowed(Hi))
