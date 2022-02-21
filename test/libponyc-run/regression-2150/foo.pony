use "debug"

trait Foo[T: Stringable #read]
  fun foo(): this->T

  fun bar() =>
    Debug.out(foo().string())
