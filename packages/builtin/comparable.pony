interface Comparable[A: Comparable[A] box]
  fun eq(that: A): Bool => this is that
  fun ne(that: A): Bool => not eq(that)

interface Ordered[A: Ordered[A] box] is Comparable[A]
  fun lt(that: A): Bool
  fun le(that: A): Bool => lt(that) or eq(that)
  fun ge(that: A): Bool => not lt(that)
  fun gt(that: A): Bool => not le(that)
