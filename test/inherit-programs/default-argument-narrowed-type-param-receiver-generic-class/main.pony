// In G[A].narrowed, a.greet() omits its argument inside iftype A <: Hi. G[Hi]
// must use the default from A's bound, Greeter, and print iface default.
interface val Greeter
  fun greet(x: String = "iface default"): String
primitive Hi is Greeter
  fun greet(x: String = "Hi default"): String => x
class G[A: Greeter val]
  new create() => None
  fun narrowed(a: A): String =>
    iftype A <: Hi then a.greet() else "-" end
actor Main
  new create(env: Env) => env.out.print(G[Hi].narrowed(Hi))
