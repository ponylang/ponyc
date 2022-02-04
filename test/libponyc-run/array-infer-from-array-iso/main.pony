trait iso T
class iso C1 is T
class iso C2 is T
class iso C3 is T

primitive Foo
  fun apply(): Array[T] iso^ =>
    [C1; C2; C3]

actor Main
  new create(env: Env) =>
    None
