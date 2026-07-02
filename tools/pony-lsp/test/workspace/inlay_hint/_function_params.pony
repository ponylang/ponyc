class _FunctionParams
  """
  Fixture for testing capability inlay hints on function parameter type
  annotations. Each method demonstrates a distinct case: missing capabilities
  on concrete types, fully explicit capabilities (no hints), and generic type
  arguments (Array[T] gets a hint; T itself does not).
  """
  // hints: " val" (after String), " val" (after U32), " ref" (after Array ']')
  fun explicit(s: String, arr: Array[U32]) =>
    None

  // no hints on parameters — capability is written out for each type
  fun full_caps(s: String box, arr: Array[U32 val] ref) =>
    None

  // hint: " ref" (after Array ']'); no hint for T — no fixed cap on type params
  fun generic[T: Comparable[T] box](arr: Array[T]) =>
    None
