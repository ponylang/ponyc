use "collections"

interface val ClassificationNotify
  """
  Receives classification statistics after a property run completes.

  Flat counts come from classify/collect/cover; tabulated counts come
  from tabulate, keyed by heading. The map arguments are read-only and
  valid only during the callback. Copy any data you need past the
  callback's return.
  """
  fun classification(
    label_counts: Map[String, USize] box,
    tabulated_counts: Map[String, Map[String, USize]] box,
    num_samples: USize)
    """
    Called once after all samples run (or after the first failure).
    """
