// g is (Greeter val | Hi). A call on a union receiver uses the default of
// its first member, Greeter; expects iface default.
interface val Greeter
  fun greet(x: String = "iface default"): String
primitive Hi is Greeter
  fun greet(x: String = "Hi default"): String => x
actor Main
  new create(env: Env) =>
    let g: (Greeter val | Hi) = Hi
    env.out.print(g.greet())
