actor GenericActor[T: Any val]
  """
  Demonstrates Find All References on a generic actor type parameter.

  Place the cursor on `T` in `actor GenericActor[T: Any val]` and invoke
  Find All References. Expect 2 locations: the declaration and the type
  annotation in the `run` behaviour parameter.
  """
  be run(x: T) =>
    None
