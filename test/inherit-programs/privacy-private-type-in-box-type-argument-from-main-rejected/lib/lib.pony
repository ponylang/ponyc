interface HasName
  fun name(): String
interface GetName
  fun get(): HasName val
primitive _H
  fun name(): String => "lib._H"
type PubH is _H
primitive MakeH
  fun apply(): PubH => _H
class val Box[B: HasName val]
  let _b: B
  new val create(b: B) => _b = b
  fun get(): B => _b
trait Foo[A: GetName val]
  fun f(a: A): String => a.get().name()
trait Mid[X: HasName val] is Foo[Box[X]]
