interface HasName
  fun name(): String
primitive _H
  fun name(): String => "lib._H"
primitive _H2
  fun name(): String => "lib._H2"
type PubH is (_H | _H2)
primitive MakeH
  fun apply(): PubH => _H
class val Wrap[A: HasName val] is HasName
  let a: A
  new val create(a': A) => a = a'
  fun name(): String => "w(" + a.name() + ")"
trait Foo[A: HasName val]
  fun show(): String => "foo"
  fun g(x: A): Foo[Wrap[A]] val => object val is Foo[Wrap[A]] fun show(): String => x.name() end
