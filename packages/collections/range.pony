class Range[A: (Real[A] val & Number) = USize] is Iterator[A]
  """
  Produces [min, max).
  """
  let _min: A
  let _max: A
  let _inc: A
  var _idx: A

  new create(min: A, max: A, inc: A = 1) =>
    _min = min
    _max = max
    _inc = inc
    _idx = min

  fun has_next(): Bool =>
    _idx < _max

  fun ref next(): A =>
    if has_next() then
      _idx = _idx + _inc
    else
      _idx
    end

  fun ref rewind() =>
    _idx = _min
