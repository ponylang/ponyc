interface HasS
  fun s(): String
primitive _S
  fun s(): String => "lib._S"
class val Box[X: HasS val = _S]
  let v: X
  new val create(v': X) => v = v'
  fun get(): X => v
primitive MakeBox
  fun apply(): Box => Box(_S)
interface Getter
  fun get(): HasS val
trait T[A: Getter val]
  fun bar(a: A): String => a.get().s()
