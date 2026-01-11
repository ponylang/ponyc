use "debug"

primitive Standard
  fun apply[T: Comparable[T] #read](
    needle: T,
    haystack: ReadSeq[T],
    ordering: Ordering = Asc)
  : (USize, Bool)
  =>
    """
    Perform a binary search for `needle` on `haystack` in the given ordering (default: Ascending).

    Returns the result as a 2-tuple of either:
    * the index of the found element and `true` if the search was successful, or
    * the index where to insert the `needle` to maintain a sorted `haystack` and `false`
    """
    try
      var i = USize(0)
      var l = USize(0)
      var r = haystack.size()
      var idx_adjustment: USize = 0
      while l < r do
        i = (l + r).fld(2)
        let elem = haystack(i)?
        match (needle.compare(elem), ordering)
        | (Less, Asc) | (Greater, Desc) =>
          idx_adjustment = 0
          r = i
        | (Equal, _) => return (i, true)
        | (Greater, Asc) | (Less, Desc) =>
          // insert index needs to be adjusted by 1 if greater
          idx_adjustment = 1
          l = i + 1
        end
      end
      (i + idx_adjustment, false)
    else
      // shouldnt happen
      Debug("invalid haystack access.")
      (0, false)
    end

  fun by[T: Comparable[T] #read, X: Any #read](
    needle: T,
    haystack: ReadSeq[X],
    retrieve: {(box->X): T} val,
    ordering: Ordering = Asc)
  : (USize, Bool)
  =>
    try
      var i = USize(0)
      var l = USize(0)
      var r = haystack.size()
      var idx_adjustment: USize = 0
      while l < r do
        i = (l + r).fld(2)
        let elem: T = retrieve(haystack(i)?)
        match (needle.compare(elem), ordering)
        | (Less, Asc) | (Greater, Desc) =>
          idx_adjustment = 0
          r = i
        | (Equal, _) => return (i, true)
        | (Greater, Asc) | (Less, Desc) =>
          idx_adjustment = 1
          l = i + 1
        end
      end
      (i + idx_adjustment, false)
    else
      // shouldnt happen
      Debug("invalid haystack access.")
      (0, false)
    end

  fun insert[T: Comparable[T] #read](
    needle: T,
    haystack: Array[T] ref,
    ordering: Ordering = Asc)
  : USize
  =>
    try
      var i = USize(0)
      var l = USize(0)
      var r = haystack.size()
      var idx_adjustment: USize = 0
      while l < r do
        i = (l + r).fld(2)
        let elem = haystack(i)?
        match (needle.compare(elem), ordering)
        | (Less, Asc) | (Greater, Desc) =>
          idx_adjustment = 0
          r = i
        | (Equal, _) =>
          haystack.insert(i, consume needle)?
          return i
        | (Greater, Asc) | (Less, Desc) =>
          idx_adjustment = 1
          l = i + 1
        end
      end
      let idx = i + idx_adjustment
      haystack.insert(idx, consume needle)?
      idx
    else
      // shouldn't happen
      Debug("invalid haystack access.")
      0
    end

