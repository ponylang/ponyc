// For C, the iftype picks Hi, and C's mk() also returns Hi, so g is a Hi.
// g.greet() must use Hi's default; expects Hi default.
interface val Greeter
  fun greet(x: String = "iface default"): String
primitive Hi is Greeter
  fun greet(x: String = "Hi default"): String => x
trait T[A: Any val]
  fun mk(): Greeter val
  fun run(): String =>
    let g = iftype A <: U8 then mk() else Hi end
    g.greet()
class C is T[Hi]
  new create() => None
  fun mk(): Hi => Hi
actor Main
  new create(env: Env) => env.out.print(C.run())
