use "collections"

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
    _iter.next()?

  new maybe(value: (A | None)) =>
    _iter =
      object is Iterator[A]
        var _value: (A | None) = consume value

        fun has_next(): Bool => _value isnt None

        fun ref next(): A ? => (_value = None) as A
      end

  new chain(outer_iterator: Iterator[Iterator[A]]) =>
    """
    Take an iterator of iterators and return an Iter containing the
    items of the first one, then the second one, and so on.

    ## Example
    ```pony
    let xs = [as I64: 1; 2].values()
    let ys = [as I64: 3; 4].values()

    Iter[I64].chain([xs; ys].values())
    ```
    `1 2 3 4`
    """
    _iter =
      object
        var inner_iterator: (Iterator[A] | None) = None

        fun ref has_next(): Bool =>
          if inner_iterator isnt None then
            try
              let iter = inner_iterator as Iterator[A]
              if iter.has_next() then
                return true
              end
            end
          end

          if outer_iterator.has_next() then
            try
              inner_iterator = outer_iterator.next()?
              return has_next()
            end
          end

          false

        fun ref next(): A ? =>
          if inner_iterator isnt None then
            let iter = inner_iterator as Iterator[A]

            if iter.has_next() then
              return iter.next()?
            end
          end

          if outer_iterator.has_next() then
            inner_iterator = outer_iterator.next()?
            return next()?
          end

          error
      end

  new repeat_value(value: A) =>
    """
    Create an iterator that returns the given value forever.

    ## Example

    ```pony
    Iter[U32].repeat_value(7)
    ```
    `7 7 7 7 7 7 7 7 7 ...`
    """
    _iter =
      object
        let _v: A = consume value

        fun ref has_next(): Bool => true

        fun ref next(): A => _v
      end

  fun ref next_or(default: A): A =>
    """
    Return the next value, or the given default.

    ## Example

    ```pony
    let x: (U64 | None) = 42
    Iter[U64].maybe(x).next_or(0)
    ```
    `42`
    """
    if has_next() then
      try next()? else default end
    else
      default
    end

  fun ref map_stateful[B](f: {ref(A!): B^ ?}): Iter[B]^ =>
    """
    Allows stateful transformation of each element from the iterator, similar
    to `map`.
    """
    Iter[B](
      object is Iterator[B]
        fun ref has_next(): Bool =>
          _iter.has_next()

        fun ref next(): B ? =>
          f(_iter.next()?)?
      end)

  fun ref filter_stateful(f: {ref(A!): Bool ?}): Iter[A!]^ =>
    """
    Allows filtering of elements based on a stateful adapter, similar to
    `filter`.
    """
    Iter[A!](
      object
        var _next: (A! | _None) = _None

        fun ref _find_next() =>
          try
            match _next
            | _None =>
              if _iter.has_next() then
                let a = _iter.next()?
                if try f(a)? else false end then
                  _next = a
                else
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

        fun ref next(): A! ? =>
          match _next = _None
          | let a: A! => a
          else
            if _iter.has_next() then
              _find_next()
              next()?
            else
              error
            end
          end
      end)

  fun ref filter_map_stateful[B](f: {ref(A!): (B^ | None) ?}): Iter[B]^ =>
    """
    Allows stateful modification to the stream of elements from an iterator,
    similar to `filter_map`.
    """
    Iter[B](
      object is Iterator[B]
        var _next: (B | _None) = _None

        fun ref _find_next() =>
          try
            match _next
            | _None =>
              if _iter.has_next() then
                match f(_iter.next()?)?
                | let b: B => _next = consume b
                | None => _find_next()
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

        fun ref next(): B ? =>
          match _next = _None
          | let b: B =>
            consume b
          else
            if _iter.has_next() then
              _find_next()
              next()?
            else
              error
            end
          end
      end)

  fun ref all(f: {(A!): Bool ?} box): Bool =>
    """
    Return false if at least one value of the iterator fails to match the
    predicate `f`. This method short-circuits at the first value where the
    predicate returns false, otherwise true is returned.

    ## Examples

    ```pony
    Iter[I64]([2; 4; 6].values())
      .all({(x) => (x % 2) == 0 })
    ```
    `true`

    ```pony
    Iter[I64]([2; 3; 4].values())
      .all({(x) => (x % 2) == 0 })
    ```
    `false`
    """
    for x in _iter do
      if not (try f(x)? else false end) then
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
    Iter[I64]([2; 4; 6].values())
      .any({(I64) => (x % 2) == 1 })
    ```
    `false`

    ```pony
    Iter[I64]([2; 3; 4].values())
      .any({(I64) => (x % 2) == 1 })
    ```
    `true`
    """
    for x in _iter do
      if try f(x)? else false end then
        return true
      end
    end
    false

  fun ref collect[B: Seq[A!] ref = Array[A!]](coll: B): B^ =>
    """
    Push each value from the iterator into the collection `coll`.

    ## Example

    ```pony
    Iter[I64]([1; 2; 3].values())
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
    Iter[I64]([1; 2; 3].values())
      .count()
    ```
    `3`
    """
    var sum: USize = 0
    for _ in _iter do
      sum = sum + 1
    end
    sum

  fun ref cycle(): Iter[A!]^ =>
    """
    Repeatedly cycle through the values from the iterator.

    WARNING: The values returned by the original iterator are cached, so
    the input iterator should be finite.

    ## Example

    ```pony
    Iter[I64]([1; 2; 3].values())
      .cycle()
    ```
    `1 2 3 1 2 3 1 2 3 ...`
    """
    let store = Array[A!]
    Iter[A!](
      object is Iterator[A!]
        let _store: Array[A!] = store
        var _store_iter: ArrayValues[A!, Array[A!]] = store.values()
        var _first_time_through: Bool = true

        fun ref has_next(): Bool => true

        fun ref next(): A! ? =>
          if _first_time_through then
            if _iter.has_next() then
              _store.push(_iter.next()?)
              return _store(_store.size() - 1)?
            end

            _first_time_through = false
          end

          if not _store_iter.has_next() then
            _store_iter.rewind()
          end

          _store_iter.next()?
      end)

  /*
  fun ref dedup[H: HashFunction[A] val = HashIs[A]](): Iter[A!]^ =>
    """
    Return an iterator that removes duplicates from consecutive identical
    elements. Equality is determined by the HashFunction `H`.

    ## Example

    ```pony
    Iter[I64]([as I64: 1; 1; 2; 3; 3; 2; 2].values())
      .dedup()
    ```
    `1 2 3 2`
    """
  */

  fun ref enum[B: (Real[B] val & Number) = USize](): Iter[(B, A)]^ =>
    """
    An iterator which yields the current iteration count as well as the next
    value from the iterator.

    ## Example

    ```pony
    Iter[I64]([1; 2; 3].values())
      .enum()
    ```
    `(0, 1) (1, 2) (2, 3)`
    """
    Iter[(B, A)](
      object
        var _i: B = 0

        fun ref has_next(): Bool => _iter.has_next()

        fun ref next(): (B, A) ? =>
          (_i = _i + 1, _iter.next()?)
      end)

  fun ref filter(f: {(A!): Bool ?} box): Iter[A!]^ =>
    """
    Return an iterator that only returns items that match the predicate `f`.

    ## Example

    ```pony
    Iter[I64]([1; 2; 3; 4; 5; 6].values())
      .filter({(x) => (x % 2) == 0 })
    ```
    `2 4 6`
    """
    filter_stateful({(a: A!): Bool ? => f(a)? })

  fun ref find(f: {(A!): Bool ?} box, n: USize = 1): A! ? =>
    """
    Return the nth value in the iterator that satisfies the predicate `f`.

    ## Examples

    ```pony
    Iter[I64]([1; 2; 3].values())
      .find({(x) => (x % 2) == 0 })
    ```
    `2`

    ```pony
    Iter[I64]([1; 2; 3; 4].values())
      .find({(x) => (x % 2) == 0 }, 2)
    ```
    `4`
    """
    var c = n
    for x in _iter do
      if try f(x)? else false end then
        if c == 1 then
          return x
        else
          c = c - 1
        end
      end
    end
    error

  fun ref filter_map[B](f: {(A!): (B^ | None) ?} box): Iter[B]^ =>
    """
    Return an iterator which applies `f` to each element. If `None` is
    returned, then the iterator will try again by applying `f` to the next
    element. Otherwise, the value of type `B` is returned.

    ## Example
    ```pony
    Iter[I64]([as I64: 1; -2; 4; 7; -5])
      .filter_map[USize](
        {(i: I64): (USize | None) => if i >= 0 then i.usize() end })
    ```
    `1 4 7`
    ```
    """
    filter_map_stateful[B]({(a: A!): (B^ | None) ? => f(a) ? })

  fun ref flat_map[B](f: {(A!): Iterator[B] ?} box): Iter[B]^ =>
    """
    Return an iterator over the values of the iterators produced from the
    application of the given function.

    ## Example

    ```pony
    Iter[String](["alpha"; "beta"; "gamma"])
      .flat_map[U8]({(s: String): Iterator[U8] => s.values() })
    ```
    `a l p h a b e t a g a m m a`
    """
    Iter[B](
      object is Iterator[B]
        var _iterb: Iterator[B] =
          try f(_iter.next()?)? else _EmptyIter[B] end

        fun ref has_next(): Bool =>
          if _iterb.has_next() then true
          else
            if _iter.has_next() then
              try
                _iterb = f(_iter.next()?)?
                has_next()
              else
                false
              end
            else
              false
            end
          end

        fun ref next(): B ? =>
          if _iterb.has_next() then
            _iterb.next()?
          else
            _iterb = f(_iter.next()?)?
            next()?
          end
      end)

  fun ref fold[B](acc: B, f: {(B, A!): B^} box): B^ =>
    """
    Apply a function to every element, producing an accumulated value.

    ## Example

    ```pony
    Iter[I64]([1; 2; 3].values())
      .fold[I64](0, {(sum, x) => sum + x })
    ```
    `6`
    """
    var acc' = consume acc
    for a in _iter do
      acc' = f(consume acc', a)
    end
    acc'

  fun ref fold_partial[B](acc: B, f: {(B, A!): B^ ?} box): B^ ? =>
    """
    A partial version of `fold`.
    """
    var acc' = consume acc
    for a in _iter do
      acc' = f(consume acc', a)?
    end
    acc'

  /*
  fun ref interleave(other: Iterator[A]): Iter[A!] =>
    """
    Return an iterator that alternates the values of the original iterator and
    the other until both run out.

    ## Example

    ```pony
    Iter[USize](Range(0, 4))
      .interleave(Range(4, 6))
    ```
    `0 4 1 5 2 3`
    """

  fun ref interleave_shortest(other: Iterator[A]): Iter[A!] =>
    """
    Return an iterator that alternates the values of the original iterator and
    the other until one of them runs out.

    ## Example

    ```pony
    Iter[USize](Range(0, 4))
      .interleave(Range(4, 6))
    ```
    `0 4 1 5 2`
    """

  fun ref intersperse(value: A, n: USize = 1): Iter[A!] =>
    """
    Return an iterator that yields the value after every `n` elements of the
    original iterator.

    ## Example
    ```pony
    Iter[USize](Range(0, 3))
      .intersperse(8)
    ```
    `0 8 1 8 2`
    """
  */

  fun ref last(): A ? =>
    """
    Return the last value of the iterator.

    ## Example

    ```pony
    Iter[I64]([1; 2; 3].values())
      .last()
    ```
    `3`
    """
    while _iter.has_next() do _iter.next()?
    else error
    end

  fun ref map[B](f: {(A!): B^ ?} box): Iter[B]^ =>
    """
    Return an iterator where each item's value is the application of the given
    function to the value in the original iterator.

    ## Example

    ```pony
    Iter[I64]([1; 2; 3].values())
      .map[I64]({(x) => x * x })
    ```
    `1 4 9`
    """
    map_stateful[B]({(a: A!): B^ ? => f(a) ? })

  fun ref nth(n: USize): A ? =>
    """
    Return the nth value of the iterator.

    ## Example

    ```pony
    Iter[I64]([1; 2; 3].values())
      .nth(2)
    ```
    `2`
    """
    var c = n
    while _iter.has_next() and (c > 1) do
      _iter.next()?
      c = c - 1
    end
    if not _iter.has_next() then
      error
    else
      _iter.next()?
    end

  fun ref run(on_error: {ref()} = {() => None } ref) =>
    """
    Iterate through the values of the iterator without a for loop. The
    function `on_error` will be called if the iterator's `has_next` method
    returns true but its `next` method throws an error.

    ## Example

    ```pony
    Iter[I64]([1; 2; 3].values())
      .map[None]({(x) => env.out.print(x.string()) })
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
      _iter.next()?
      run(on_error)
    else
      on_error()
    end

  fun ref skip(n: USize): Iter[A]^ =>
    """
    Skip the first n values of the iterator.

    ## Example

    ```pony
    Iter[I64]([1; 2; 3; 4; 5; 6].values())
      .skip(3)
    ```
    `4 5 6`

    ```pony
    Iter[I64]([1; 2; 3].values())
      .skip(3)
      .has_next()
    ```
    `false`
    """
    var c = n
    try
      while _iter.has_next() and (c > 0) do
        _iter.next()?
        c = c - 1
      end
    end
    this

  fun ref skip_while(f: {(A!): Bool ?} box): Iter[A!]^ =>
    """
    Skip values of the iterator while the predicate `f` returns true.

    ## Example

    ```pony
    Iter[I64]([1; 2; 3; 4; 5; 6].values())
      .skip_while({(x) => x < 4 })
    ```
    `4 5 6`
    """
    filter_stateful(
      object
        var _done: Bool = false

        fun ref apply(a: A!): Bool =>
          if _done then return true end
          if try f(a)? else false end then
            false
          else
            _done = true
            true
          end
      end)

  fun ref take(n: USize): Iter[A]^ =>
    """
    Return an iterator for the first n elements.

    ## Example

    ```pony
    Iter[I64]([1; 2; 3; 4; 5; 6].values())
      .take(3)
    ```
    `1 2 3`
    """
    Iter[A](
      object
        var _countdown: USize = n

        fun ref has_next(): Bool =>
          (_countdown > 0) and _iter.has_next()

        fun ref next(): A ? =>
          if _countdown > 0 then
            _countdown = _countdown - 1
            _iter.next()?
          else
            error
          end
      end)

  fun ref take_while(f: {(A!): Bool ?} box): Iter[A!]^ =>
    """
    Return an iterator that returns values while the predicate `f` returns
    true. This iterator short-circuits the first time that `f` returns false or
    raises an error.

    ## Example

    ```pony
    Iter[I64]([1; 2; 3; 4; 5; 6].values())
      .take_while({(x) => x < 4 })
    ```
    `1 2 3`
    """
    Iter[A!](
      object
        var _done: Bool = false
        var _next: (A! | None) = None

        fun ref has_next(): Bool =>
          if _next isnt None then true
          else _try_next()
          end

        fun ref next(): A! ? =>
          if (_next isnt None) or _try_next() then
            (_next = None) as A!
          else error
          end

        fun ref _try_next(): Bool =>
          if _done then false
          elseif not _iter.has_next() then
            _done = true
            false
          else
            _next =
              try _iter.next()?
              else
                _done = true
                return false
              end
            _done = try not f(_next as A!)? else true end
            not _done
          end
      end)

  /*
  fun ref unique[H: HashFunction[A] val = HashIs[A]](): Iter[A!]^ =>
    """
    Return an iterator that filters out elements that have already been
    produced. Uniqueness is determined by the HashFunction `H`.

    ## Example

    ```pony
    Iter[I64]([as I64: 1; 2; 1; 1; 3; 4; 1].values())
        .unique()
    ```
    `1 2 3 4`
    """
  */

  fun ref zip[B](i2: Iterator[B]): Iter[(A, B)]^ =>
    """
    Zip two iterators together so that each call to next() results in
    a tuple with the next value of the first iterator and the next value
    of the second iterator. The number of items returned is the minimum of
    the number of items returned by the two iterators.

    ## Example

    ```pony
    Iter[I64]([1; 2].values())
      .zip[I64]([3; 4].values())
    ```
    `(1, 3) (2, 4)`
    """
    Iter[(A, B)](
      object is Iterator[(A, B)]
        let _i1: Iterator[A] = _iter
        let _i2: Iterator[B] = i2

        fun ref has_next(): Bool =>
          _i1.has_next() and _i2.has_next()

        fun ref next(): (A, B) ? =>
          (_i1.next()?, _i2.next()?)
      end)

  fun ref zip2[B, C](i2: Iterator[B], i3: Iterator[C]): Iter[(A, B, C)]^ =>
    """
    Zip three iterators together so that each call to next() results in
    a tuple with the next value of the first iterator, the next value
    of the second iterator, and the value of the third iterator. The
    number of items returned is the minimum of the number of items
    returned by the three iterators.
    """
    Iter[(A, B, C)](
      object is Iterator[(A, B, C)]
        let _i1: Iterator[A] = _iter
        let _i2: Iterator[B] = i2
        let _i3: Iterator[C] = i3

        fun ref has_next(): Bool =>
          _i1.has_next() and _i2.has_next() and _i3.has_next()

        fun ref next(): (A, B, C) ? =>
          (_i1.next()?, _i2.next()?, _i3.next()?)
      end)

  fun ref zip3[B, C, D](i2: Iterator[B], i3: Iterator[C], i4: Iterator[D])
    : Iter[(A, B, C, D)]^
  =>
    """
    Zip four iterators together so that each call to next() results in
    a tuple with the next value of each of the iterators. The number of
    items returned is the minimum of the number of items returned by the
    iterators.
    """
    Iter[(A, B, C, D)](
      object is Iterator[(A, B, C, D)]
        let _i1: Iterator[A] = _iter
        let _i2: Iterator[B] = i2
        let _i3: Iterator[C] = i3
        let _i4: Iterator[D] = i4

        fun ref has_next(): Bool =>
          _i1.has_next()
            and _i2.has_next()
            and _i3.has_next()
            and _i4.has_next()

        fun ref next(): (A, B, C, D) ? =>
          (_i1.next()?, _i2.next()?, _i3.next()?, _i4.next()?)
      end)

  fun ref zip4[B, C, D, E](
    i2: Iterator[B],
    i3: Iterator[C],
    i4: Iterator[D],
    i5: Iterator[E])
    : Iter[(A, B, C, D, E)]^
  =>
    """
    Zip five iterators together so that each call to next() results in
    a tuple with the next value of each of the iterators. The number of
    items returned is the minimum of the number of items returned by the
    iterators.
    """
    Iter[(A, B, C, D, E)](
      object is Iterator[(A, B, C, D, E)]
        let _i1: Iterator[A] = _iter
        let _i2: Iterator[B] = i2
        let _i3: Iterator[C] = i3
        let _i4: Iterator[D] = i4
        let _i5: Iterator[E] = i5

        fun ref has_next(): Bool =>
          _i1.has_next()
            and _i2.has_next()
            and _i3.has_next()
            and _i4.has_next()
            and _i5.has_next()

        fun ref next(): (A, B, C, D, E) ? =>
          (_i1.next()?, _i2.next()?, _i3.next()?, _i4.next()?, _i5.next()?)
      end)

primitive _None

class _EmptyIter[A]
  fun ref has_next(): Bool => false
  fun ref next(): A ? => error
