primitive Sort[A: Seq[B] ref, B: Comparable[B] #read]
  """
  Implementation of introsort: dual-pivot quicksort with a heapsort
  fallback when recursion depth exceeds O(log n). Sorts in-place, using
  a small amount of additional memory. Elements must satisfy the
  `Comparable` interface.

  Dual-pivot quicksort partitions around two pivots into three regions,
  making efficient use of modern processor caches. When the quicksort
  partitioning degenerates (e.g. on already-sorted, reverse-sorted, or
  all-equal input), the affected subarray is heapsorted instead,
  keeping the worst case at O(n log n).

  ## Example program
  The following sorts a reverse-alphabetical array of Strings in place.

  It outputs:

  > first
  > second
  > third

  ```pony
  use "collections"

  actor Main
    new create(env:Env) =>
      let array = [ "third"; "second"; "first" ]
      let sorted_array = Sort[Array[String], String](array)
      for e in sorted_array.values() do
        env.out.print(e) // prints "first \n second \n third"
      end
  ```
  """
  fun apply(a: A): A^ =>
    """
    Sort the given seq.
    """
    try
      let n = a.size()
      let depth = _log2(n) * 2
      _sort(a, 0, n.isize() - 1, depth)?
    end
    a

  fun _log2(n: USize): USize =>
    var x = n
    var log: USize = 0
    while x > 1 do
      x = x >> 1
      log = log + 1
    end
    log

  fun _sort(a: A, lo: ISize, hi: ISize, depth: USize) ? =>
    if hi <= lo then return end
    if depth == 0 then
      _heapsort(a, lo, hi)?
      return
    end
    // choose outermost elements as pivots
    if a(lo.usize())? > a(hi.usize())? then _swap(a, lo, hi)? end
    (var p, var q) = (a(lo.usize())?, a(hi.usize())?)
    // partition according to invariant
    (var l, var g) = (lo + 1, hi - 1)
    var k = l
    while k <= g do
      if a(k.usize())? < p then
        _swap(a, k, l)?
        l = l + 1
      elseif a(k.usize())? >= q then
        while (a(g.usize())? > q) and (k < g) do g = g - 1 end
        _swap(a, k, g)?
        g = g - 1
        if a(k.usize())? < p then
          _swap(a, k, l)?
          l = l + 1
        end
      end
      k = k + 1
    end
    (l, g) = (l - 1, g + 1)
    // swap pivots to final positions
    _swap(a, lo, l)?
    _swap(a, hi, g)?
    // recursively sort 3 partitions
    _sort(a, lo, l - 1, depth - 1)?
    _sort(a, l + 1, g - 1, depth - 1)?
    _sort(a, g + 1, hi, depth - 1)?

  fun _heapsort(a: A, lo: ISize, hi: ISize) ? =>
    let n = (hi - lo) + 1
    // build max-heap
    var i = (n / 2) - 1
    while i >= 0 do
      _sift_down(a, i, n, lo)?
      i = i - 1
    end
    // extract elements from heap
    var end_idx = n - 1
    while end_idx > 0 do
      _swap(a, lo, lo + end_idx)?
      _sift_down(a, 0, end_idx, lo)?
      end_idx = end_idx - 1
    end

  fun _sift_down(a: A, start: ISize, count: ISize, lo: ISize) ? =>
    var root = start
    var child = (2 * root) + 1
    while child < count do
      if ((child + 1) < count) and
        (a((lo + child).usize())? < a((lo + child + 1).usize())?)
      then
        child = child + 1
      end
      if a((lo + root).usize())? < a((lo + child).usize())? then
        _swap(a, lo + root, lo + child)?
        root = child
        child = (2 * root) + 1
      else
        return
      end
    end

  fun _swap(a: A, i: ISize, j: ISize) ? =>
    a(j.usize())? = a(i.usize())? = a(j.usize())?
