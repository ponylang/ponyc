interface HasName
  fun name(): String
primitive _H
  fun name(): String => "lib._H"
type PubH is _H
primitive MakeH
  fun apply(): PubH => _H
trait Foo[A: HasName val]
  fun look(y: A): String => "foo"
  fun g(): Foo[A] val => object val is Foo[A] fun look(y: A): String => y.name() end
