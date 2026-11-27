primitive _H
  fun name(): String => "lib._H"
trait T
  fun p(x: String = _H.name()): String => x
