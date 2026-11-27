// rot() rotates T's 64 type arguments by one, so 64 calls return to the
// start. C.rot().rot().name() must print "t": no limit on how many types
// such a cycle passes through may stop the program.
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
primitive P18
primitive P19
primitive P20
primitive P21
primitive P22
primitive P23
primitive P24
primitive P25
primitive P26
primitive P27
primitive P28
primitive P29
primitive P30
primitive P31
primitive P32
primitive P33
primitive P34
primitive P35
primitive P36
primitive P37
primitive P38
primitive P39
primitive P40
primitive P41
primitive P42
primitive P43
primitive P44
primitive P45
primitive P46
primitive P47
primitive P48
primitive P49
primitive P50
primitive P51
primitive P52
primitive P53
primitive P54
primitive P55
primitive P56
primitive P57
primitive P58
primitive P59
primitive P60
primitive P61
primitive P62
primitive P63
trait T[A0: Any val, A1: Any val, A2: Any val, A3: Any val, A4: Any val, A5: Any val, A6: Any val, A7: Any val, A8: Any val, A9: Any val, A10: Any val, A11: Any val, A12: Any val, A13: Any val, A14: Any val, A15: Any val, A16: Any val, A17: Any val, A18: Any val, A19: Any val, A20: Any val, A21: Any val, A22: Any val, A23: Any val, A24: Any val, A25: Any val, A26: Any val, A27: Any val, A28: Any val, A29: Any val, A30: Any val, A31: Any val, A32: Any val, A33: Any val, A34: Any val, A35: Any val, A36: Any val, A37: Any val, A38: Any val, A39: Any val, A40: Any val, A41: Any val, A42: Any val, A43: Any val, A44: Any val, A45: Any val, A46: Any val, A47: Any val, A48: Any val, A49: Any val, A50: Any val, A51: Any val, A52: Any val, A53: Any val, A54: Any val, A55: Any val, A56: Any val, A57: Any val, A58: Any val, A59: Any val, A60: Any val, A61: Any val, A62: Any val, A63: Any val]
  fun name(): String => "t"
  fun rot(): T[A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30, A31, A32, A33, A34, A35, A36, A37, A38, A39, A40, A41, A42, A43, A44, A45, A46, A47, A48, A49, A50, A51, A52, A53, A54, A55, A56, A57, A58, A59, A60, A61, A62, A63, A0] val => object val is T[A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15, A16, A17, A18, A19, A20, A21, A22, A23, A24, A25, A26, A27, A28, A29, A30, A31, A32, A33, A34, A35, A36, A37, A38, A39, A40, A41, A42, A43, A44, A45, A46, A47, A48, A49, A50, A51, A52, A53, A54, A55, A56, A57, A58, A59, A60, A61, A62, A63, A0] end
class C is T[P0, P1, P2, P3, P4, P5, P6, P7, P8, P9, P10, P11, P12, P13, P14, P15, P16, P17, P18, P19, P20, P21, P22, P23, P24, P25, P26, P27, P28, P29, P30, P31, P32, P33, P34, P35, P36, P37, P38, P39, P40, P41, P42, P43, P44, P45, P46, P47, P48, P49, P50, P51, P52, P53, P54, P55, P56, P57, P58, P59, P60, P61, P62, P63]
  new create() => None
actor Main
  new create(env: Env) => env.out.print(C.rot().rot().name())
