trait Comparable[A]
  fun eq{var|val}(that:A):Bool
  fun ne{var|val}(that:A):Bool = not eq(that)

trait Ordered[A] is Comparable[A]
  fun lt{var|val}(that:A):Bool
  fun le{var|val}(that:A):Bool = lt(that) or eq(that)
  fun ge{var|val}(that:A):Bool = not lt(that)
  fun gt{var|val}(that:A):Bool = not le(that)
