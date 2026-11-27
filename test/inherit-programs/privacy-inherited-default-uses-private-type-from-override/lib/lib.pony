interface val HasY
  fun y(): String
trait T
  fun foo(): HasY val
  fun bar(): String => foo().y()
