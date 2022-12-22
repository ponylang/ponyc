class Range[A: (Real[A] val & Number) = USize] is Iterator[A]
  """
  Produces `[min, max)` with a step of `inc` for any `Number` type.

  ```pony
  // iterating with for-loop
  for i in Range(0, 10) do
    env.out.print(i.string())
  end

  // iterating over Range of U8 with while-loop
  let range = Range[U8](5, 100, 5)
  while range.has_next() do
    try
      handle_u8(range.next()?)
    end
  end
  ```

  Supports `min` being smaller than `max` with negative `inc`
  but only for signed integer types and floats:

  ```pony
  var previous = 11
  for left in Range[I64](10, -5, -1) do
    if not (left < previous) then
      error
    end
    previous = left
  end
  ```

  If `inc` is nonzero, but cannot produce progress towards `max` because of its sign, the `Range` is considered empty and will not produce any iterations. The `Range` is also empty if either `min` equals `max`, independent of the value of `inc`, or if `inc` is zero.

  ```pony
  let empty_range1 = Range(0, 10, -1)
  let empty_range2 = Range(0, 10, 0)
  let empty_range3 = Range(10, 10)
  empty_range1.is_empty() == true
  empty_range2.is_empty() == true
  empty_range3.is_empty() == true
  ```

  Note that when using unsigned integers, a negative literal wraps around so while `Range[ISize](0, 10, -1)` is empty as above, `Range[USize](0, 10, -1)` produces a single value of `min` or `[0]` here.

  When using `Range` with floating point types (`F32` and `F64`) `inc` steps < 1.0 are possible. If any arguments contains NaN, the `Range` is considered empty. It is also empty if the lower bound `min` or the step `inc` are +Inf or -Inf. However, if only the upper bound `max` is +Inf or -Inf and the step parameter `inc` has the same sign, then the `Range` is considered infinite and will iterate indefinitely.

  ```pony
  let p_inf: F64 = F64.max_value() + F64.max_value()
  let n_inf: F64 = -p_inf
  let nan: F64 = F64(0) / F64(0)

  let infinite_range1 = Range[F64](0, p_inf, 1)
  let infinite_range2 = Range[F64](0, n_inf, -1_000_000)
  infinite_range1.is_infinite() == true
  infinite_range2.is_infinite() == true

  for i in Range[F64](0.5, 100, nan) do
    // will not be executed as `inc` is nan
  end
  for i in Range[F64](0.5, 100, p_inf) do
    // will not be executed as `inc` is +Inf
  end
  ```
  """
  let _min: A
  let _max: A
  let _inc: A
  let _forward: Bool
  let _infinite: Bool
  let _empty: Bool
  var _idx: A

  new create(min: A, max: A, inc: A = 1) =>
    _min = min
    _max = max
    _inc = inc
    _forward = (_min < _max) and (_inc > 0)
    (let min_finite, let max_finite, let inc_finite) =
      iftype A <: FloatingPoint[A] then
        (_min.finite(), _max.finite(), _inc.finite())
      else
        (true, true, true)
      end
    let progress = ((_min < _max) and (_inc > 0))
                    or ((_min > _max) and (_inc < 0)) // false if any is NaN!
    if progress and min_finite and inc_finite then
      _empty = false
      _infinite = not max_finite // ok to use not max_finite for max_infinite
                                 // since progress excludes _max == nan
      _idx = _min
    else
      _empty = true
      _infinite = false
      _idx = _max // has_next() will return false without code modification
    end

  fun has_next(): Bool =>
    if _infinite then
      return true
    end
    if _forward then
      _idx < _max
    else
      _idx > _max
    end

  fun ref next(): A ? =>
    if has_next() then
      _idx = _idx + _inc
    else
      error
    end

  fun ref rewind() =>
    _idx = _min

  fun is_infinite(): Bool =>
    _infinite

  fun is_empty(): Bool =>
    _empty
