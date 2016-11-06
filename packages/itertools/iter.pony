
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

    ## Example
    ```pony
    let xs = [as I64: 1, 2].values()
    let ys = [as I64: 3, 4].values()

    Iter[I64].chain([xs, ys].values())
    ```
    `1 2 3 4`
    """
    _iter = Chain[A](outer_iterator)

  fun ref all(f: {(A!): Bool ?} box): Bool =>
    """
    Return false if at least one value of the iterator fails to match the
    predicate `f`. This method short-circuits at the first value where the
    predicate returns false, otherwise true is returned.

    ## Examples

    ```pony
    Iter[I64]([as I64: 2, 4, 6].values())
      .all({(x: I64): Bool => (x % 2) == 0 })
    ```
    `true`

    ```pony
    Iter[I64]([as I64: 2, 3, 4].values())
      .all({(x: I64): Bool => (x % 2) == 0 })
    ```
    `false`
    """
    for x in _iter do
      if not (try f(x) else false end) then
        return false
      end
    end
    true

  fun ref any(f: {(A!): Bool ?} box): Bool =>
    """
    Return true if at least one value of the iterator matches the predicate 
    `f`. This method short-circuits at the first value where the predicate
    returns true, otherwise false is returned.

    ## Examples

    ```pony
    Iter[I64]([as I64: 2, 4, 6].values())
      .any({(x: I64): Bool => (x % 2) == 1 })
    ```
    `false`

    ```pony
    Iter[I64]([as I64: 2, 3, 4].values())
      .any({(x: I64): Bool => (x % 2) == 1 })
    ```
    `true`
    """
    for x in _iter do
      if try f(x) else false end then
        return true
      end
    end
    false

  fun ref collect[B: Seq[A!] = Array[A!]](coll: B): B^ =>
    """
    Push each value from the iterator into the collection `coll`.
    
    ## Example

    ```pony
    Iter[I64]([as I64: 1, 2, 3].values())
      .collect(Array[I64](3))
    ```
    `[1, 2, 3]`
    """
    for x in _iter do
      coll.push(x)
    end
    coll

  fun ref count(): USize =>
    """
    Return the number of values in the iterator.

    ## Example

    ```pony
    Iter[I64]([as I64: 1, 2, 3].values())
      .count()
    ```
    `3`
    """
    var c: USize = 0
    for x in _iter do
      c = c + 1
    end
    c

  fun ref cycle(): Iter[A!]^ =>
    """
    Repeatedly cycle through the values from the iterator.

    WARNING: The values returned by the original iterator are cached, so
    the input iterator should be finite.

    ## Example

    ```pony
    Iter[I64]([as I64: 1, 2, 3].values())
      .cycle()
    ```
    `1 2 3 1 2 3 1 2 3 ...`
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

    ## Example

    ```pony
    Iter[I64]([as I64: 1, 2, 3].values())
      .enum()
    ```
    `(0, 1) (1, 2) (2, 3)`
    """
    Iter[(B, A)](object is Iterator[(B, A)]
      let _iter: Iterator[A] = _iter
      var _i: B = 0

      fun ref has_next(): Bool =>
        _iter.has_next()

      fun ref next(): (B, A) ? =>
        (_i = _i + 1, _iter.next())

    end)

  fun ref filter(f: {(A!): Bool ?} box): Iter[A]^ =>
    """
    Return an iterator that only returns items that match the predicate `f`.
    
    ## Example

    ```pony
    Iter[I64]([as I64: 1, 2, 3, 4, 5, 6].values())
      .filter({(x: I64): Bool => (x % 2) == 0 })
    ```
    `2 4 6`
    """
    Iter[A](object is Iterator[A]
      let _fn: {(A!): Bool ?} box = f
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

  fun ref find(f: {(A!): Bool ?} box, n: USize = 1): A ? =>
    """
    Return the nth value in the iterator that satisfies the predicate `f`.
    
    ## Examples

    ```pony
    Iter[I64]([as I64: 1, 2, 3].values())
      .find({(x: I64): Bool => (x % 2) == 0 })
    ```
    `2`

    ```pony
    Iter[I64]([as I64: 1, 2, 3, 4].values())
      .find({(x: I64): Bool => (x % 2) == 0 }, 2)
    ```
    `4`
    """
    var c = n
    for x in _iter do
      if try f(x) else false end then
        if c == 1 then
          return consume x as A
        else
          c = c - 1
        end
      end
    end
    error

  fun ref fold[B](f: {(B, A!): B^ ?} box, acc: B): B^ ? =>
    """
    Apply a function to every element, producing an accumulated value.
    
    ## Example

    ```pony
    Iter[I64]([as I64: 1, 2, 3].values())
      .fold[I64]({(x: I64, sum: I64): I64 => sum + x }, 0)
    ```
    `6`
    """
    if not _iter.has_next() then
      return acc
    end
    fold[B](f, f(consume acc, _iter.next()))

  fun ref last(): A ? =>
    """
    Return the last value of the iterator.

    ## Example

    ```pony
    Iter[I64]([as I64: 1, 2, 3].values())
      .last()
    ```
    `3`
    """
    var l = _iter.next()
    while _iter.has_next() do
      l = _iter.next()
    end
    consume l as A

  fun ref map[B](f: {(A!): B ?} box): Iter[B]^ =>
    """
    Return an iterator where each item's value is the application of the given
    function to the value in the original iterator.
    
    ## Example

    ```pony
    Iter[I64]([as I64: 1, 2, 3].values())
      .map[I64]({(x: I64): I64 => x * x })
    ```
    `1 4 9`
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

  fun ref nth(n: USize): A ? =>
    """
    Return the nth value of the iterator.

    ## Example

    ```pony
    Iter[I64]([as I64: 1, 2, 3].values())
      .nth(2)
    ```
    `2`
    """
    var c = n
    while c > 1 do
      _iter.next()
      c = c - 1
    end
    _iter.next()

  fun ref run(on_error: {()} box = {() => None }) =>
    """
    Iterate through the values of the iterator without a for loop. The
    function `on_error` will be called if the iterator's `has_next` method
    returns true but its `next` method trows an error.

    ## Example

    ```pony
    Iter[I64]([as I64: 1, 2, 3].values())
      .map[None]({(x: I64)(env) => env.out.print(x.string()) })
      .run()
    ```
    ```
    1
    2
    3
    ```
    """
    if not _iter.has_next() then return end
    try 
      _iter.next() 
      run(on_error)
    else
      on_error()
    end

  fun ref skip(n: USize): Iter[A]^ =>
    """
    Skip the first n values of the iterator.

    ## Example

    ```pony
    Iter[I64]([as I64: 1, 2, 3, 4, 5, 6].values())
      .skip(3)
    ```
    `4 5 6`
    """
    var c = n
    try
      while c > 0 do
        _iter.next()
        c = c - 1
      end
    end
    this

  fun ref skip_while(f: {(A!): Bool ?} box): Iter[A]^ =>
    """
    Skip values of the iterator while the predicate `f` returns true.
    
    ## Example

    ```pony
    Iter[I64]([as I64: 1, 2, 3, 4, 5, 6].values())
      .skip_while({(x: I64): Bool => x < 4 })
    ```
    `4 5 6`
    """
    Iter[A](object is Iterator[A]
      let _f: {(A!): Bool ?} box = f
      let _iter: Iterator[A] = _iter
      var _done: Bool = false

      fun ref has_next(): Bool =>
        _iter.has_next()

      fun ref next(): A ? =>
        if _done then
          _iter.next()
        else
          let x = _iter.next()
          if try _f(x) else false end then
            next()
          else
            _done = true
            consume x as A
          end
        end

    end)

  fun ref take(n: USize): Iter[A]^ =>
    """
    Return an iterator for the first n elements.

    ## Example

    ```pony
    Iter[I64]([as I64: 1, 2, 3, 4, 5, 6].values())
      .take(3)
    ```
    `1 2 3`
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

  fun ref take_while(f: {(A!): Bool ?} box): Iter[A]^ =>
    """
    Return an iterator that returns values while the predicate `f` returns
    true.

    ## Example

    ```pony
    Iter[I64]([as I64: 1, 2, 3, 4, 5, 6].values())
      .take_while({(x: I64): Bool => x < 4 })
    ```
    `1 2 3`
    """
    let pred = object
      let _f: {(A!): Bool ?} box = f
      var _taking: Bool = true

      fun apply(a: A!): Bool ? =>
        if _taking then
          _f(a)
        else
          false
        end

    end
    filter(pred)

  fun ref zip[B](i2: Iterator[B]): Iter[(A, B)]^ =>
    """
    Zip two iterators together so that each call to next() results in the
    a tuple with the next value of the first iterator and the next value
    of the second iterator. The number of items returned is the minimum of
    the number of items returned by the two iterators.

    ## Example

    ```pony
    Iter[I64]([as I64: 1, 2].values())
      .zip[I64]([as I64: 3, 4].values())
    ```
    `(1, 3) (2, 4)`
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
