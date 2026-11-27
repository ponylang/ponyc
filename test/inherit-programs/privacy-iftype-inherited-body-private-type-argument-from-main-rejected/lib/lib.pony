interface HasName
  fun name(): String
primitive _H
  fun name(): String => "lib._H"
type PubH is _H
primitive MakeH
  fun apply(): PubH => _H
trait Foo[A: Any val]
  fun f(x: A): String =>
    iftype A <: HasName val then x.name() else "none" end
