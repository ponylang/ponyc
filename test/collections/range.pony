class Range[A:Arithmetic = I64] is Iterator[A]
  val min: A
  val max: A
  val step: A
  var idx: A

  new create(min': A, max': A) ->
    min := min'
    max := max'
    step := 1
    idx := min

  new step(min': A, max': A, step': A) ->
    min := min'
    max := max'
    step := step'
    idx := min

  fun:box has_next(): Bool -> idx < max

  fun:var? next(): this.A -> if idx < max then idx := idx + step else undef end

  fun:var rewind() -> idx := min
