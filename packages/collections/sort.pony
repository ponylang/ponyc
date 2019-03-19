primitive Sort[A: Seq[B] ref, B: Comparable[B] #read]
  """
  Implementation of dual-pivot quicksort.
  
  ## Example program
  The following takes an reverse-alphabetical array of Strings ("third", "second", "first"), 
  and sorts it in place alphabetically using the default String Comparator.
  
  It outputs:
    
    first
    second
    third
  
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
    try _sort(a, 0, a.size().isize() - 1)? end
    a

  fun _sort(a: A, lo: ISize, hi: ISize) ? =>
    if hi <= lo then return end
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
    _sort(a, lo, l - 1)?
    _sort(a, l + 1, g - 1)?
    _sort(a, g + 1, hi)?

  fun _swap(a: A, i: ISize, j: ISize) ? =>
    a(j.usize())? = a(i.usize())? = a(j.usize())?
