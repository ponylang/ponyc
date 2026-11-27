primitive _Y
  fun y(): String => "l2._Y secret"
trait G[A: Any val]
  fun make(): A
  fun get(): A => make()
trait val Thing is G[_Y]
  fun make(): _Y => _Y
primitive Impl is Thing
