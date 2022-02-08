trait iso T
class iso C1 is T
class iso C2 is T
class iso C3 is T

interface ArrayishOfArrayOfT
  fun values(): Iterator[this->Array[T]]^

primitive Foo
  fun apply(): ArrayishOfArrayOfT =>
    [[C1]; [C2]; [C3]]

actor Main
  new create(env: Env) =>
    None
