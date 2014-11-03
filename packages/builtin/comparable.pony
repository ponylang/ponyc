trait Comparable[A: Comparable[A] box]
  fun box eq(that: A): Bool => this is that
  fun box ne(that: A): Bool => not eq(that)

trait Ordered[A: Ordered[A] box] is Comparable[A]
  fun box lt(that: A): Bool
  fun box le(that: A): Bool => lt(that) or eq(that)
  fun box ge(that: A): Bool => not lt(that)
  fun box gt(that: A): Bool => not le(that)
