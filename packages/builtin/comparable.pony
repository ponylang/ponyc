trait Comparable[T: Comparable[T]]
  fun box eq(that: T box): Bool => this is that
  fun box ne(that: T box): Bool => not eq(that)

trait Ordered[A: Ordered[A]] is Comparable[A]
  fun box lt(that: A box): Bool
  fun box le(that: A box): Bool => lt(that) or eq(that)
  fun box ge(that: A box): Bool => not lt(that)
  fun box gt(that: A box): Bool => not le(that)
