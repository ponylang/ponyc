primitive _L
  fun x(): String => "lib._L"
type PubL is _L
primitive MakeL
  fun apply(): PubL => _L
trait T
  fun foo(): _L => _L
  fun bar(): String => foo().x()
