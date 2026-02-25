primitive _BinarySearch
  """
  Binary search for a needle in a sorted haystack (ascending order).

  Returns a 2-tuple of either:
  * the index of the found element and `true` if the search was successful, or
  * the index where to insert the `needle` to maintain a sorted `haystack`
    and `false`
  """
  fun apply[T: Comparable[T] #read](
    needle: T,
    haystack: ReadSeq[T])
  : (USize, Bool)
  =>
    try
      var i = USize(0)
      var l = USize(0)
      var r = haystack.size()
      var idx_adjustment: USize = 0
      while l < r do
        i = (l + r).fld(2)
        let elem = haystack(i)?
        match needle.compare(elem)
        | Less =>
          idx_adjustment = 0
          r = i
        | Equal => return (i, true)
        | Greater =>
          idx_adjustment = 1
          l = i + 1
        end
      end
      (i + idx_adjustment, false)
    else
      (-1, false)
    end
