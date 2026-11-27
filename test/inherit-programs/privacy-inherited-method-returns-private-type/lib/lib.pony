primitive _L
  fun x(): String => "lib._L"
trait T
  fun foo(): _L => _L
  fun bar(): String => foo().x()
