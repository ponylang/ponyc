class Foo is Comparable[Foo]
  fun ref compare(that: Foo): Bool =>
    this == that

  fun box eq(that: Foo box): Bool =>
    this == that
