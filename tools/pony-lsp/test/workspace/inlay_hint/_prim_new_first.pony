primitive _PrimNewFirst
  """
  Fixture for the case where a primitive's first member is a constructor.
  Synthetic ne inherits position at 'n' of 'new', coinciding with ne's own
  first char. The boundary-byte guard must distinguish them.
  """
  new create() =>
    None

  // hints: " box" (receiver), " val" (None return)
  fun greet(): None =>
    None
