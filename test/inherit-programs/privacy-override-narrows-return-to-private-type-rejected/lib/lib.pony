primitive _H is Stringable
  fun string(): String iso^ => "lib._H".clone()
type PubH is _H
primitive MakeH
  fun apply(): PubH => _H
trait T
  fun foo(): Stringable
  fun bar(): String => foo().string()
