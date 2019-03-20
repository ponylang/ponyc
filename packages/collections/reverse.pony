class Reverse[A: (Real[A] val & Number) = USize] is Iterator[A]
  """
  Produces a decreasing range [max, min] with step `dec`, for any `Number` type.
  (i.e. the reverse of `Range`)

  Example program: 

  ```pony
  use "collections"
  actor Main
    new create(env: Env) =>
      for e in Reverse(10, 2, 2) do
        env.out.print(e.string())
      end 
  ```
  Which outputs: 
  ```
  10
  8
  6
  4
  2
  ```

  If `dec` is 0, produces an infinite series of `max`.

  If `dec` is negative, produces a range with `max` as the only value.

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
    (_idx >= _min) and (_idx <= _max)

  fun ref next(): A =>
    if has_next() then
      _idx = _idx - _dec
    else
      _idx + _dec
    end

  fun ref rewind() =>
    _idx = _max
