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
    handle_u8(range.next())
  end
  ```

  Supports `min` being smaller than `max` with negative `inc`:

  ```pony
  var previous = 11
  for left in Range[I64](10, -5, -1) do
    if not (left < previous) then
      error
    end
    previous = left
  end
  ```

  If the `step` is not moving `min` towards `max` or if it is `0`,
  the Range is considered infinite and iterating over it
  will never terminate:

  ```pony
  let infinite_range1 = Range(0, 1, 0)
  infinite_range1.is_infinite() == true

  let infinite_range2 = Range[U8](0, 10, -1)
  for _ in infinite_range2 do
    env.out.print("will this ever end?")
    env.err.print("no, never!")
  end
  ```

  When using `Range` with  floating point types (`F32` and `F64`)
  `inc` steps < 1.0 are possible. If any of the arguments contains
  `NaN`, `+Inf` or `-Inf` the range is considered infinite operations on
  any of them don't move `min` towards `max`.
  The actual values produced by the range are determined by what IEEE 754
  defines as the repeated result of `min` + `inc`:

  ```pony
  for and_a_half in Range[F64](0.5, 100) do
    handle_half(and_a_half)
  end

  // this Range will produce 0 at first, then infinitely NaN
  let nan: F64 = F64(0) / F64(0)
  for what_am_i in Range[F64](0, 1000, nan) do
    wild_guess(what_am_i)
  end
  ```

  """
  let _min: A
  let _max: A
  let _inc: A
  let _forward: Bool
  let _infinite: Bool
  var _idx: A

  new create(min: A, max: A, inc: A = 1) =>
    _min = min
    _max = max
    _inc = inc
    _idx = min
    _forward = (_min < _max) and (_inc > 0)
    let is_float_infinite =
      iftype A <: FloatingPoint[A] then
        _min.nan() or _min.infinite()
          or _max.nan() or _max.infinite()
          or _inc.nan() or _inc.infinite()
      else
        false
      end
    _infinite =
      is_float_infinite
        or ((_inc == 0) and (min != max))    // no progress
        or ((_min < _max) and (_inc < 0)) // progress into other directions
        or ((_min > _max) and (_inc > 0))

  fun has_next(): Bool =>
    if _forward then
      _idx < _max
    else
      _idx > _max
    end

  fun ref next(): A =>
    if has_next() then
      _idx = _idx + _inc
    else
      _idx
    end

  fun ref rewind() =>
    _idx = _min

  fun is_infinite(): Bool =>
    _infinite
