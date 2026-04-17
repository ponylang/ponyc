class \nodoc\ ForExpressions
  """
  Test fixture for for-loop expression-level folding ranges.
  Verifies that fold ranges work for for loops with multi-statement bodies
  and nested expressions, via the tk_while arm after sugar_for() desugaring.
  """
  fun with_multi_statement_for(arr: Array[U32] box): U32 =>
    var sum: U32 = 0
    for item in arr.values() do
      let doubled = item * 2
      sum = sum + doubled
    end
    sum

  fun with_nested_for(arr: Array[U32] box): U32 =>
    var result: U32 = 0
    for item in arr.values() do
      if (item % 2) == 0 then
        result = result + item
      end
    end
    result
