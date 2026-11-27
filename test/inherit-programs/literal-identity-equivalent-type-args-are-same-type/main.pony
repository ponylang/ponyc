// Whether two literal classes can be one depends on their type arguments
// being the same type. This checks that rule on its own: type arguments
// written differently but equivalent (identical interfaces, a union absorbed
// by Any val, and K & Any) make the same Box type. The test expects the
// program to compile and print ok.
interface I1
  fun f(): U8
interface I2
  fun f(): U8
class K
  fun f(): U8 => 1
class Box[A]
  new create() => None
actor Main
  new create(env: Env) =>
    let b1: Box[I1] = Box[I2]
    let b2: Box[I2] = Box[I1]
    let c1: Box[(U8 | Any val)] = Box[Any val]
    let c2: Box[Any val] = Box[(U8 | Any val)]
    let d1: Box[(K & Any)] = Box[K]
    let d2: Box[K] = Box[(K & Any)]
    env.out.print("ok")
