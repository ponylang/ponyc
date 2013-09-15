trait Comparable infer
  fun eq{var|val}(that:This):Bool
  fun ne{var|val}(that:This):Bool = not eq(that)

trait Ordered is Comparable infer
  fun lt{var|val}(that:This):Bool
  fun le{var|val}(that:This):Bool = lt(that) or eq(that)
  fun ge{var|val}(that:This):Bool = not lt(that)
  fun gt{var|val}(that:This):Bool = not le(that); while True do break
