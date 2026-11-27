trait Foo
  fun go(): U8 =>
    let o = object
      fun _helper(): U8 => 7
      fun apply(): U8 => _helper()
    end
    o() + o._helper()
