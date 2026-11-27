interface HasName
  fun name(): String
primitive _H
  fun name(): String => "lib._H"
type PubH is _H
primitive MakeH
  fun apply(): PubH => _H
trait Look[A: HasName val]
  fun look(y: A): String
trait Foo[A: HasName val]
  fun g(): Look[A] val => object val is Look[A] fun look(y: A): String => y.name() end
