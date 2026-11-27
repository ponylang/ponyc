interface val HasName
  fun name(): String
interface val GetName
  fun get(): HasName val
primitive _H
  fun name(): String => "lib._H"
class val Wrapper[B: HasName val]
  let _b: B
  new val create(b: B) => _b = b
  fun get(): B => _b
type PubW is Wrapper[_H]
primitive MakeW
  fun apply(): PubW => Wrapper[_H](_H)
trait Foo[A: GetName val]
  fun f(a: A): String => a.get().name()
