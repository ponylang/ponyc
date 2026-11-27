primitive _Y
  fun y(): String => "l2._Y secret"
type PubY is _Y
primitive Thing
  fun get(): PubY => _Y
