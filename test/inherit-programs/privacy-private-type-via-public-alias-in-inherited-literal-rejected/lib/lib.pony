interface HasName
  fun name(): String
primitive _H
  fun name(): String => "lib._H"
type PubH is _H
primitive MakeH
  fun apply(): PubH => _H
interface Show
  fun show(): String
trait Foo[A: HasName val]
  fun k(): Foo[A] val => object val is Foo[A] end
  fun g(x: A): Show val => object val is Show fun show(): String => x.name() end
