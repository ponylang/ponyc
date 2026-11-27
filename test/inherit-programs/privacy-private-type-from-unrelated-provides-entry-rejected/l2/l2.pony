primitive _Y
  fun y(): String => "l2._Y secret"
trait G[A: Any val]
  fun make(): A
  fun get(): A => make()
trait H is G[_Y]
  fun make(): _Y => _Y
