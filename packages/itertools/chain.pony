class Chain[A] is Iterator[A]
  """
  Take an iterator of iterators and work through them, returning the
  items of the first one, then the second one, and so on.

  ## Example program

  Print the numbers 1 through 14

  ```pony
  use "itertools"

  actor Main
    new create(env: Env) =>
      let i1 = [as I32: 1, 2, 3, 4]
      let i2 = [as I32: 5, 6, 7, 8, 9]
      let i3 = [as I32: 10, 11, 12, 13, 14]
    
      for x in Chain[I32]([i1.values(), i2.values(), i3.values()].values()) do
        env.out.print(x.string())
      end
  ```
  """
  let _outer_iterator: Iterator[Iterator[A]]
  var _inner_iterator: (Iterator[A] | None) = None

  new create(outer_iterator: Iterator[Iterator[A]]) =>
    _outer_iterator = outer_iterator

  fun ref has_next(): Bool =>
    if _inner_iterator isnt None then
      try
        let iter = _inner_iterator as Iterator[A]
        if iter.has_next() then
          return true
        end
      end
    end

    if _outer_iterator.has_next() then
      try
        _inner_iterator = _outer_iterator.next()
        return has_next()
      end
    end

    false

  fun ref next(): A ? =>
    if _inner_iterator isnt None then
      let iter = _inner_iterator as Iterator[A]

      if iter.has_next() then
        return iter.next()
      end
    end

    if _outer_iterator.has_next() then
      _inner_iterator = _outer_iterator.next()
      return next()
    end

    error
