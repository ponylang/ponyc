class _DsContainment
  """
  Fixture for `document_symbol/integration/containment` and
  `document_symbol/integration/range`. Layout mirrors
  `references/referenced_class.pony` so `fun ref increment` lands on a
  known line, giving the range test a meaningful min_end_line below the
  keyword-only span.
  """
  var _count: U32 = 0

  new create() =>
    _count = 0

  fun ref increment(): U32 =>
    _count = _count + 1
    _count

  fun maybe(): (U32 | None) =>
    None
