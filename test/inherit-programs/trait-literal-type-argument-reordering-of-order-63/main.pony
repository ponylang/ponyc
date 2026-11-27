// rot() reorders T's sixteen type arguments as a 7-cycle and a 9-cycle, so 63
// calls return to the start. C.rot().rot().name() must print "t": no limit
// on how many types such a cycle passes through may stop the program.
primitive P0
primitive P1
primitive P2
primitive P3
primitive P4
primitive P5
primitive P6
primitive P7
primitive P8
primitive P9
primitive P10
primitive P11
primitive P12
primitive P13
primitive P14
primitive P15
trait T[A0: Any val, A1: Any val, A2: Any val, A3: Any val, A4: Any val, A5: Any val, A6: Any val, A7: Any val, A8: Any val, A9: Any val, A10: Any val, A11: Any val, A12: Any val, A13: Any val, A14: Any val, A15: Any val]
  fun name(): String => "t"
  fun rot(): T[A1, A2, A3, A4, A5, A6, A0, A8, A9, A10, A11, A12, A13, A14, A15, A7] val => object val is T[A1, A2, A3, A4, A5, A6, A0, A8, A9, A10, A11, A12, A13, A14, A15, A7] end
class C is T[P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.rot().rot().name())
