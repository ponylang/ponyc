class \nodoc\ IfdefExpressions
  """
  Test fixture for ifdef expression-level folding ranges.
  """
  fun with_ifdef(x: U32): U32 =>
    ifdef debug then
      x + 1
    else
      x
    end
