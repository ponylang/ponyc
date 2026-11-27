interface HasName
  fun name(): String
primitive _H
  fun name(): String => "lib._H"
type PubH is _H
primitive MakeH
  fun apply(): PubH => _H
trait Show
  fun show(): String
trait Foo[A: HasName val]
  fun g(): Foo2[A] val => object val is Foo2[A] end
trait Foo2[A: HasName val]
  fun h(x: A): Show val => object val is Show fun show(): String => x.name() end
