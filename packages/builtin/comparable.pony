trait Comparable[A]
  fun eq{box}(that:A):Bool
  fun ne{box}(that:A):Bool = not eq(that)

trait Ordered[A] is Comparable[A]
  fun lt{box}(that:A):Bool
  fun le{box}(that:A):Bool = lt(that) or eq(that)
  fun ge{box}(that:A):Bool = not lt(that)
  fun gt{box}(that:A):Bool = not le(that)
