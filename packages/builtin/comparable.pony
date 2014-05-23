trait Comparable[A: Comparable[A]]
  fun box eq(that: A box): Bool => this is that
  fun box ne(that: A box): Bool => not eq(that)

trait Ordered[A: Ordered[A]] is Comparable[A]
  fun box lt(that: A box): Bool
  fun box le(that: A box): Bool => lt(that) or eq(that)
  fun box ge(that: A box): Bool => not lt(that)
  fun box gt(that: A box): Bool => not le(that)
  fun box min(that: A box): A => if lt(that) then this else that end
  fun box max(that: A box): A => if ge(that) then this else that end
