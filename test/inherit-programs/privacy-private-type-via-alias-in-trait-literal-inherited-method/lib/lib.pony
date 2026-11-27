interface HasName
  fun name(): String
primitive _H
  fun name(): String => "lib._H"
type PubH is _H
primitive MakeH
  fun apply(): PubH => _H
trait Foo[A: HasName val]
  fun show(x: A): String => x.name()
  fun mk(): Foo[A] val => object val is Foo[A] end
