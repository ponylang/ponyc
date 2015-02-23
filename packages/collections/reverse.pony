class Reverse[A: (Real[A] box & Number) = U64] is Iterator[A]
  """
  Produces (max, min].
  """
  let _min: A
  let _max: A
  let _dec: A
  var _idx: A

  new create(max: A, min: A, dec: A = 1) =>
    _min = min
    _max = max
    _dec = dec
    _idx = max

  fun has_next(): Bool =>
    _idx >= (_min + _dec)

  fun ref next(): A =>
    if _idx >= (_min + _dec) then
      _idx = _idx - _dec
    end
    _idx

  fun ref rewind() =>
    _idx = _max
