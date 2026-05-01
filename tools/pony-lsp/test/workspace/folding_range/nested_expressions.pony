class \nodoc\ NestedExpressions
  """
  Test fixture for nested and sequential compound expression folding ranges.
  """
  fun with_nested(n: U32): U32 =>
    var i: U32 = 0
    while i < n do
      if (i % 2) == 0 then
        i = i + 2
      else
        i = i + 1
      end
    end
    i

  fun with_sequential(n: U32): U32 =>
    var result: U32 = 0
    if n > 0 then
      result = n
    else
      result = 0
    end
    while result > 0 do
      result = result - 1
    end
    result
