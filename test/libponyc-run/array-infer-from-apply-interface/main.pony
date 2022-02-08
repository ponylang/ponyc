trait iso T
class iso C1 is T
class iso C2 is T
class iso C3 is T

interface ArrayishOfT
  fun apply(index: USize): this->T ?

primitive Foo
  fun apply(): ArrayishOfT =>
    [C1; C2; C3]

actor Main
  new create(env: Env) =>
    None
