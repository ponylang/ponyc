class _UnionTypes
  """
  Tests for cap hints on union and tuple type annotations.
  """
  fun demo() =>
    // hints: " val" (U32), " val" (None)
    let u: (U32 | None) = 0
    // hints: " val" (U32), " val" (Bool)
    let t: (U32, Bool) = (0, false)
    // hints: " val" (None), " box" (Stringable)
    let i: (None & Stringable) = None
    None

actor _BeNewExclusion
  be run(x: String) =>
    None

  new create(x: U32) =>
    None
