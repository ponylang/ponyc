class Range[A: (Real[A] val & Number) = USize] is Iterator[A]
  """
  A bounded numeric sequence with a given step size. Two named constructors
  control whether the far bound is included:

  * `Range.till` — excludes the far bound
  * `Range.through` — includes the far bound

  Direction is inferred: when `from < to` the range ascends, when `from > to`
  it descends. Step is always a positive magnitude.

  A zero or negative step, or a non-finite float argument (NaN or infinity),
  produces an empty range.

  ```pony
  // 0, 1, 2, 3, 4
  for i in Range.till(0, 5) do
    env.out.print(i.string())
  end

  // 5, 4, 3, 2, 1, 0
  for i in Range.through(5, 0) do
    env.out.print(i.string())
  end

  // with a step size
  for i in Range.till(0, 10, 2) do
    env.out.print(i.string())  // 0, 2, 4, 6, 8
  end
  ```
  """
  let _from: A
  let _to: A
  let _step: A
  // `through` sets true when from <= to; `till` when from < to
  let _forward: Bool
  let _inclusive: Bool
  // true when constructor args are invalid; rewind cannot clear this
  let _empty: Bool
  var _idx: A
  // set when _idx wraps past the type boundary, preventing infinite loops
  var _done: Bool = false

  new through(from: A, to: A, step: A = 1) =>
    """
    Inclusive of the far bound.

    `Range.through(0, 3)` yields 0, 1, 2, 3.
    `Range.through(3, 0)` yields 3, 2, 1, 0.
    `Range.through(5, 5)` yields 5.
    """
    _from = from
    _to = to
    _step = step
    _forward = from <= to
    _inclusive = true
    _idx = from
    _empty =
      if step <= 0 then true
      else
        iftype A <: FloatingPoint[A] then
          (not from.finite()) or (not to.finite()) or (not step.finite())
        else
          false
        end
      end

  new till(from: A, to: A, step: A = 1) =>
    """
    Exclusive of the far bound.

    `Range.till(0, 3)` yields 0, 1, 2.
    `Range.till(3, 0)` yields 3, 2, 1.
    `Range.till(5, 5)` yields nothing.
    """
    _from = from
    _to = to
    _step = step
    _forward = from < to
    _inclusive = false
    _idx = from
    _empty =
      if step <= 0 then true
      else
        iftype A <: FloatingPoint[A] then
          (not from.finite()) or (not to.finite()) or (not step.finite())
        else
          false
        end
      end

  fun has_next(): Bool =>
    """
    True when the iterator has not yet reached the far bound.
    """
    if _empty or _done then return false end
    if _forward then
      if _inclusive then
        (_idx >= _from) and (_idx <= _to)
      else
        (_idx >= _from) and (_idx < _to)
      end
    else
      if _inclusive then
        (_idx <= _from) and (_idx >= _to)
      else
        (_idx <= _from) and (_idx > _to)
      end
    end

  fun ref next(): A ? =>
    """
    Return the current value and advance. Errors when exhausted.
    """
    if has_next() then
      let current = _idx
      if _forward then
        _idx = current + _step
        if _idx < current then _done = true end
      else
        _idx = current - _step
        if _idx > current then _done = true end
      end
      current
    else
      error
    end

  fun ref rewind() =>
    """
    Reset the iterator to its starting position.
    """
    _idx = _from
    _done = false
