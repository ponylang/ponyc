interface HasName
  fun name(): String
primitive _H
  fun name(): String => "lib._H"
type PubH is _H
primitive MakeH
  fun apply(): PubH => _H
class val Wrap[A: HasName val] is HasName
  let a: A
  new val create(a': A) => a = a'
  fun name(): String => "w(" + a.name() + ")"
class val Box[A: HasName val]
  let a: A
  new val create(a': A) => a = a'
  fun show(): String => a.name()
trait Foo[A: HasName val]
  fun show(): String => "foo"
  fun g(x: A): Foo[Wrap[A]] val =>
    object val is Foo[Wrap[A]]
      fun show(): String =>
        let xx: A = x
        let f = {(): String => xx.name()} val
        f()
    end
