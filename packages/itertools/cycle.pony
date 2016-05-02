class Cycle[A] is Iterator[A]
  """
  Create an iterator that repeatedly cycles through the values from the
  original iterator.

  WARNING: The values returned by the original iterator are cached, so
  the input iterator should be finite.

  ## Example program

  Print the numbers 1 through 4 forever.

  ```pony
  actor Main
    new create(env: Env) =>
      let i1 = [as I32: 1, 2, 3, 4]
      for x in Cycle[I32](i1.values()) do
        env.out.print(x.string())
      end
  ```
  """
  let _store: Array[A] = Array[A]
  var _store_iter: ArrayValues[A, Array[A]] = _store.values()
  let _iter: Iterator[A^]
  var _first_time_through: Bool = true

  new create(iter: Iterator[A^]) =>
    _iter = iter

  fun ref has_next(): Bool =>
    true

  fun ref next(): A ? =>
    if _first_time_through then
      if _iter.has_next() then
        _store.push(_iter.next())
        return _store(_store.size() - 1)
      end

      _first_time_through = false
    end

    if not _store_iter.has_next() then
      _store_iter.rewind()
    end

    _store_iter.next()
