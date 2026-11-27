interface HasY
  fun y(): String
trait S[B: HasY val]
  fun bar(a: B): String
trait T2[A: HasY val]
  fun bar(a: A): String => a.y()
interface I2[A: HasY val]
  fun baz(a: A): String => a.y()
