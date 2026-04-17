class Foldable
  """
  A class with members to test folding ranges.
  """
  var _x: U32 = 0

  new create(x: U32) =>
    _x = x

  fun get_value(): U32 =>
    _x

  fun single_line(): U32 => _x
