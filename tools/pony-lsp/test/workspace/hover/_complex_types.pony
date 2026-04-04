class _ComplexTypes
  """
  Demonstrates that hover works on complex type expressions.
  Union types, tuple types, and intersection types are formatted correctly.
  """

  fun demo_complex_types(): String =>
    """
    Hover over the variable names on their declaration lines to see formatted
    types. Hover shows 'let union_type: (String | U32 | None)' and similar.
    """
    let union_type: (String | U32 | None) = "test"
    let tuple_type: (String, U32, Bool) = ("test", U32(42), true)
    union_type.string() + tuple_type._1.string()
