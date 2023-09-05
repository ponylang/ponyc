class Bar[T: Stringable #read] is Foo[T]
  let _t: T

  new create(t: T) =>
    _t = t

  fun foo(): this->T =>
    _t
