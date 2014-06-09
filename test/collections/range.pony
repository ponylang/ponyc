class Range[A: Arithmetic = I64] is Iterator[A]
  val min: A
  val max: A
  val inc: A
  var idx: A

  new create(min': A, max': A) =>
    min = min'
    max = max'
    inc = 1
    idx = min

  new step(min': A, max': A, inc': A) =>
    min = min'
    max = max'
    inc = inc'
    idx = min

  fun box has_next(): Bool => idx < max

  fun mut next(): this.A ? => if idx < max then idx = idx + inc else error end

  fun mut rewind() => idx = min
