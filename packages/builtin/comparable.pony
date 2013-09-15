trait Comparable infer
  fun eq{var|val}(that:This):Bool
  fun lt{var|val}(that:This):Bool
  fun ne{var|val}(that:This):Bool = not eq(that)
  fun le{var|val}(that:This):Bool = eq(that) or lt(that)
  fun ge{var|val}(that:This):Bool = not lt(that)
  fun gt{var|val}(that:This):Bool = not le(that)
