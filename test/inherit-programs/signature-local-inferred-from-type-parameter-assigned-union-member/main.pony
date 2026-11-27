// T[A] infers locals from a parameter of type A and reassigns them. C provides
// T[(U8 | None)] and calls g(1, None), assigning None to a local first set
// from 1. The local must keep type A, so the program prints "okok2".
trait T[A: Any val]
  fun f(a: A): String =>
    var x = a
    x = (consume x)
    "ok"
  fun g(a: A, b: A): String =>
    var x = a
    x = b
    "ok2"
class C is T[(U8 | None)]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.f(1) + C.g(1, None))
