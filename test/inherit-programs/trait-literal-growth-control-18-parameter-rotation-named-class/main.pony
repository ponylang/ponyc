// A trait with 18 type parameters declares rot(), which returns the trait with
// its arguments rotated, and named class R implements it. The rotations repeat
// after a finite number of steps. The test expects "t".
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
primitive P16
primitive P17
trait T[A0: Any val, A1: Any val, A2: Any val, A3: Any val, A4: Any val, A5: Any val, A6: Any val, A7: Any val, A8: Any val, A9: Any val, A10: Any val, A11: Any val, A12: Any val, A13: Any val, A14: Any val, A15: Any val, A16: Any val, A17: Any val]
  fun name(): String => "t"
  fun rot(): T[A1, A2, A3, A4, A0, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A5] val
class val R[A0: Any val, A1: Any val, A2: Any val, A3: Any val, A4: Any val, A5: Any val, A6: Any val, A7: Any val, A8: Any val, A9: Any val, A10: Any val, A11: Any val, A12: Any val, A13: Any val, A14: Any val, A15: Any val, A16: Any val, A17: Any val] is T[A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17]
  new val create() => None
  fun rot(): T[A1, A2, A3, A4, A0, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A5] val => R[A1, A2, A3, A4, A0, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A5]
class C is T[P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15, P16, P17]
  new create() => None
  fun rot(): T[P1, P2, P3, P4, P0, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15, P16, P17, P5] val => R[P1, P2, P3, P4, P0, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15, P16, P17, P5]
actor Main
  new create(env: Env) => env.out.print(C.rot().rot().name())
