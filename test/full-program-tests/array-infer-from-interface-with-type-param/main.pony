trait iso T
class iso C1 is T
class iso C2 is T
class iso C3 is T

interface Arrayish[A]
  fun apply(index: USize): this->A ?

primitive Foo
  fun apply(): Arrayish[T] =>
    [C1; C2; C3]

actor Main
  new create(env: Env) =>
    None
