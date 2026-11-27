primitive _Y
  fun y(): String => "l2._Y secret"
trait G[A: Any val]
  fun mk(): A
  fun get(): A => mk()
primitive Thing is G[_Y]
  fun mk(): _Y => _Y
