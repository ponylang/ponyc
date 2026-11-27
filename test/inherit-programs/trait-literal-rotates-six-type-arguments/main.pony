// T's six type arguments are rotated by one in rot()'s literal, so six calls
// return to the start. C.rot().rot().name() must print "t".
primitive P0
primitive P1
primitive P2
primitive P3
primitive P4
primitive P5
trait T[A0: Any val, A1: Any val, A2: Any val, A3: Any val, A4: Any val, A5: Any val]
  fun name(): String => "t"
  fun rot(): T[A1, A2, A3, A4, A5, A0] val => object val is T[A1, A2, A3, A4, A5, A0] end
class C is T[P0, P1, P2, P3, P4, P5]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.rot().rot().name())
