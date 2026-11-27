use l = "../lib"
primitive _Y
  fun y(): String => "l2._Y"
trait X is (l.T2[_Y] & l.S[_Y])
primitive Run
  fun apply(c: X): String => c.bar(_Y)
