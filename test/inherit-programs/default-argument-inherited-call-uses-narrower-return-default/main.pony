// C.g() returns Hi where T says Greeter. C's copy of run() calls greet() on Hi,
// so it uses Hi's default; through T, greet() uses Greeter's default.
// Expects Hi default, then iface default.
interface Greeter
  fun greet(x: String = "iface default"): String
class Hi is Greeter
  new create() => None
  fun greet(x: String = "Hi default"): String => x
trait T
  fun g(): Greeter
  fun run(): String => g().greet()
class C is T
  new create() => None
  fun g(): Hi => Hi
actor Main
  new create(env: Env) =>
    env.out.print(C.run())
    let t: T = C
    env.out.print(t.g().greet())
