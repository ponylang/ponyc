class Range[A: (Real[A] & Number) = I64] is Iterator[A]
  let min: A
  let max: A
  let inc: A
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

  fun ref next(): A^ => if idx < max then idx = idx + inc else idx end

  fun ref rewind() => idx = min

  fun ref identity(): Range[A] => this
