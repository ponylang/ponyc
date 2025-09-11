// from issue #2233
trait box T
class box C1 is T
  new box create() => None
class box C2 is T
  new box create() => None
class box C3 is T
  new box create() => None

interface ArrayishOfTAndU64
  fun apply(index: USize): this->(T, U64) ?

primitive Foo
  fun apply(): ArrayishOfTAndU64 =>
    [(C1, 1); (C2, 2); (C3, 3)]

actor Main
  new create(env: Env) =>
    None
