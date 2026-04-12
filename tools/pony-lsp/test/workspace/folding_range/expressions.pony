class Expressions
  """
  Test fixture for expression-level folding ranges.
  Contains one method per compound expression type.
  """
  fun with_if(x: U32): U32 =>
    if x > 0 then
      x + 1
    else
      0
    end

  fun with_match(x: U32): String =>
    match x
    | 0 => "zero"
    | 1 => "one"
    else
      "many"
    end

  fun with_while(x: U32): U32 =>
    var i: U32 = 0
    while i < x do
      i = i + 1
    end
    i

  fun with_try(arr: Array[U32] box): U32 =>
    try
      arr(0)?
    else
      0
    end
