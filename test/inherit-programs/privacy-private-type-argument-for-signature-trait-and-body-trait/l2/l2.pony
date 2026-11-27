use l = "../lib"
primitive _Y
  fun y(): String => "l2._Y"
trait X is (l.S[_Y] & l.T2[_Y])
primitive Run
  fun apply(c: X): String => c.bar(_Y)
