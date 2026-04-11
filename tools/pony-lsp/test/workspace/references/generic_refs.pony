class GenericRefs[T]
  """
  Demonstrates Find All References on a generic type parameter.

  Place the cursor on `T` in `class GenericRefs[T]` and invoke Find All
  References. Expect 3 locations: the declaration and both type annotations
  in `id` (the parameter type `x: T` and the return type `): T`).

  With includeDeclaration disabled, expect 2 locations: both type
  annotations in `id` only.

  Placing the cursor on either type annotation in `id` instead of the
  declaration produces the same set of locations.
  """
  fun id(x: T): T =>
    x
