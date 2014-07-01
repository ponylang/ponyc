class Foo is Ordered[Foo]
  var x: U32

  fun ref order(that: Foo): Bool =>
    this < that

  fun box lt(that: Foo box): Bool =>
    /*x < that.x*/
    x < 10
