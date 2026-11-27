primitive _X
  fun x(): String => "lib._X"
primitive Lib
  fun make(): _X => _X
trait T
  fun bar(): String => Lib.make().x()
