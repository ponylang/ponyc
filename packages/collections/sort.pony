primitive Sort[A: Comparable[A] val]
  """
  Implementation of a 3-way partition QuickSort.
  """
  fun apply(a: Seq[A]): Seq[A]^ ? =>
    """
    Sort the given seq.
    """
    _sort(a, 0, a.size())
    a

  fun _sort(a: Seq[A], start: USize, stop: USize) ? =>
    if (stop - start) < 2 then return end
    let key = a(start + ((stop - start) / 2))
    (var e, var u, var g) = (start, start, stop)
    while u < g do
      if a(u) < key then
        _swap(a, u, e)
        e = e + 1
        u = u + 1
      elseif a(u) == key then
        u = u + 1
      else
        g = g - 1
        _swap(a, u, g)
      end
    end
    _sort(a, start, e)
    _sort(a, g, stop)

  fun _swap(a: Seq[A], i: USize, j: USize) ? =>
    let tmp = a(i)
    a(i) = a(j)
    a(j) = tmp
