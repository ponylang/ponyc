interface HasName
  fun name(): String
primitive _H1
  fun name(): String => "p1._H1"
type PubH1 is _H1
primitive MakeH1
  fun apply(): PubH1 => _H1
trait T1[A: HasName val]
  fun f(x: A): String => x.name()
