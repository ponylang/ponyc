class _TypeInference
  """
  Demonstrates that type inference works in hover.

  Hover shows inferred types on variable declarations even without explicit
  annotations.
  """

  fun demo_type_inference(): String =>
    """
    Hover shows inferred types on variable DECLARATIONS!
    Try hovering over the variable names on their declaration lines.

    ACTUAL: Hover shows 'let inferred_bool: Bool' even without explicit type
    This works because the type-checked AST contains inferred type info.
    """

    // Hover shows: let inferred_string: String val
    let inferred_string = "test"

    // Hover shows: let inferred_bool: Bool
    let inferred_bool = true

    // Hover shows: let inferred_array: Array[U32] ref
    let inferred_array = Array[U32]

    inferred_string + inferred_bool.string() + inferred_array.size().string()
