interface Comparable[A: Comparable[A] box]
  fun eq(that: box->A): Bool => this is that
  fun ne(that: box->A): Bool => not eq(that)

interface Ordered[A: Ordered[A] box] is Comparable[A]
  fun lt(that: box->A): Bool
  fun le(that: box->A): Bool => lt(that) or eq(that)
  fun ge(that: box->A): Bool => not lt(that)
  fun gt(that: box->A): Bool => not le(that)
