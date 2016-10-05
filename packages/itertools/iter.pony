
class Iter[A] is Iterator[A]
  """
  Wrapper class containing methods to modify iterators.
  """
  let _iter: Iterator[A]

  new create(iter: Iterator[A]) =>
    _iter = iter

  fun ref has_next(): Bool =>
    _iter.has_next()

  fun ref next(): A ? =>
    _iter.next()

  new chain(outer_iterator: Iterator[Iterator[A]]) =>
    """
    Take an iterator of iterators and work through them, returning the
    items of the first one, then the second one, and so on.
    """
    _iter = Chain[A](outer_iterator)

  fun ref cycle(): Iter[A!] =>
    """
    Repeatedly cycle through the values from the iterator.

    WARNING: The values returned by the original iterator are cached, so
    the input iterator should be finite.
    """
    let store = Array[A!]
    Iter[A!](object is Iterator[A!]
      let _store: Array[A!] = store
      var _store_iter: ArrayValues[A!, Array[A!]] = store.values()
      let _iter: Iterator[A] = _iter
      var _first_time_through: Bool = true

      fun ref has_next(): Bool =>
        true

      fun ref next(): A! ? =>
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

    end)

  fun ref enum[B: (Real[B] val & Number) = USize](): Iter[(B, A)]^ =>
    """
    An iterator which yields the current iteration count as well as the next
    value from the iterator.
    """
    Iter[(B, A)](object is Iterator[(B, A)]
      let _iter: Iterator[A] = _iter
      var _i: B = 0

      fun ref has_next(): Bool =>
        _iter.has_next()

      fun ref next(): (B, A) ? =>
        (_i = _i + 1, _iter.next())

    end)

  fun ref filter(fn: {(A!): Bool ?} box): Iter[A]^ =>
    """
    Take a predicate function and return an iterator that only returns items
    that match the predicate.
    """
    Iter[A](object is Iterator[A]
      let _fn: {(A!): Bool ?} box = fn
      let _iter: Iterator[A] = _iter
      var _next: (A! | _None) = _None

      fun ref _find_next() =>
        try
          match _next
          | _None =>
            if _iter.has_next() then
              _next = _iter.next()
              if try not _fn(_next as A) else true end then
                _next = _None
                _find_next()
              end
            end
          end
        end

      fun ref has_next(): Bool =>
        match _next
        | _None =>
          if _iter.has_next() then
            _find_next()
            has_next()
          else
            false
          end
        else
          true
        end

      fun ref next(): A ? =>
        match _next
        | let v: A =>
          _next = _None
          consume v
        else
          if _iter.has_next() then
            _find_next()
            next()
          else
            error
          end
        end
    end)

  fun ref fold[B](f: {(B, A!): B^ ?} box, acc: B): B^ ? =>
    """
    Apply a function to every element, producing an accumulated value.
    """
    if not _iter.has_next() then
      return acc
    end
    fold[B](f, f(consume acc, _iter.next()))

  fun ref map[B](f: {(A!): B ?} box): Iter[B] =>
    """
    Return an iterator where each item's value is the application of the given
    function to the value in the original iterator.
    """
    Iter[B](object is Iterator[B]
      let _iter: Iterator[A] = _iter
      let _f: {(A!): B ?} box = f

      fun ref has_next(): Bool =>
        _iter.has_next()

      fun ref next(): B ? =>
        try
          _f(_iter.next())
        else
          if _iter.has_next() then
            next()
          else
            error
          end
        end
    end)

  fun ref take(n: USize): Iter[A] =>
    """
    Only return a specified number of items.
    """
    Iter[A](object is Iterator[A]
      var _countdown: USize = n
      let _iter: Iterator[A] = _iter

      fun ref has_next(): Bool =>
        (_countdown > 0) and _iter.has_next()

      fun ref next(): A ? =>
        _countdown = _countdown - 1
        _iter.next()
    end)

  fun ref zip[B](i2: Iterator[B]): Iter[(A, B)]^ =>
    """
    Zip two iterators together so that each call to next() results in the
    a tuple with the next value of the first iterator and the next value
    of the second iterator. The number of items returned is the minimum of
    the number of items returned by the two iterators.
    """
    Iter[(A, B)](object is Iterator[(A, B)]
      let _i1: Iterator[A] = _iter
      let _i2: Iterator[B] = i2

      fun ref has_next(): Bool =>
        _i1.has_next() and _i2.has_next()

      fun ref next(): (A, B) ? =>
        (_i1.next(), _i2.next())
    end)

  fun ref zip2[B, C](i2: Iterator[B], i3: Iterator[C]): Iter[(A, B, C)]^ =>
    """
    Zip three iterators together so that each call to next() results in
    the a tuple with the next value of the first iterator, the next value
    of the second iterator, and the value of the third iterator. The
    number of items returned is the minimum of the number of items
    returned by the three iterators.
    """
    Iter[(A, B, C)](object is Iterator[(A, B, C)]
      let _i1: Iterator[A] = _iter
      let _i2: Iterator[B] = i2
      let _i3: Iterator[C] = i3

      fun ref has_next(): Bool =>
        _i1.has_next() and _i2.has_next() and _i3.has_next()

      fun ref next(): (A, B, C) ? =>
        (_i1.next(), _i2.next(), _i3.next())
    end)

  fun ref zip3[B, C, D](i2: Iterator[B], i3: Iterator[C], i4: Iterator[D]):
    Iter[(A, B, C, D)]^
  =>
    """
    Zip four iterators together so that each call to next() results in the
    a tuple with the next value of each of the iterators. The number of
    items returned is the minimum of the number of items returned by the
    iterators.
    """
    Iter[(A, B, C, D)](object is Iterator[(A, B, C, D)]
      let _i1: Iterator[A] = _iter
      let _i2: Iterator[B] = i2
      let _i3: Iterator[C] = i3
      let _i4: Iterator[D] = i4

      fun ref has_next(): Bool =>
        _i1.has_next() and _i2.has_next() and _i3.has_next() and _i4.has_next()

      fun ref next(): (A, B, C, D) ? =>
        (_i1.next(), _i2.next(), _i3.next(), _i4.next())
    end)

  fun ref zip4[B, C, D, E](i2: Iterator[B], i3: Iterator[C], i4: Iterator[D],
    i5: Iterator[E]): Iter[(A, B, C, D, E)]^
  =>
    """
    Zip five iterators together so that each call to next() results in the
    a tuple with the next value of each of the iterators. The number of
    items returned is the minimum of the number of items returned by the
    iterators.
    """
    Iter[(A, B, C, D, E)](object is Iterator[(A, B, C, D, E)]
      let _i1: Iterator[A] = _iter
      let _i2: Iterator[B] = i2
      let _i3: Iterator[C] = i3
      let _i4: Iterator[D] = i4
      let _i5: Iterator[E] = i5

      fun ref has_next(): Bool =>
        _i1.has_next() and _i2.has_next() and _i3.has_next() and _i4.has_next()
          and _i5.has_next()

      fun ref next(): (A, B, C, D, E) ? =>
        (_i1.next(), _i2.next(), _i3.next(), _i4.next(), _i5.next())
    end)
