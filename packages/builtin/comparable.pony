interface Comparable[A: Comparable[A] box]
  fun eq(that: this->A): Bool => this is that
  fun ne(that: this->A): Bool => not eq(that)

interface Ordered[A: Ordered[A] box] is Comparable[A]
  fun lt(that: this->A): Bool
  fun le(that: this->A): Bool => lt(that) or eq(that)
  fun ge(that: this->A): Bool => not lt(that)
  fun gt(that: this->A): Bool => not le(that)
