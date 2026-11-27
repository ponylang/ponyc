interface HasName
  fun name(): String
primitive _H
  fun name(): String => "lib._H"
type PubH is _H
primitive MakeH
  fun apply(): PubH => _H
class val Box[A: HasName val]
  let a: A
  new val create(a': A) => a = a'
  fun show(): String => a.name()
trait Foo[A: HasName val]
  fun f(x: A): String => x.name()
