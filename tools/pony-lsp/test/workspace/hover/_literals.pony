class _Literals
  """
  Demonstrates that hover on literal values returns no result.
  Literals have types but are not named entities with hover information.
  """

  fun demo_literals() =>
    """
    Try hovering over the literal values below.
    Hover returns no result for literal values.
    """
    let integer: U32 = 42
    let hex: U32 = 0xFF
    let binary: U32 = 0b1010
    let float_val: F64 = 3.14
    let string_val: String = "hello"
    let char_val: U32 = 'A'
    let bool_true: Bool = true
    let bool_false: Bool = false
    let array_val: Array[U32] = [42; 0; 7]
