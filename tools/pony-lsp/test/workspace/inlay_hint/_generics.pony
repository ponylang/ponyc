class _Generics
  """
  Tests for inlay hints on generic type annotations.
  """
  fun demo(): USize =>
    // hints: " val" (U32), " ref" (Array)
    let arr_u32: Array[U32] = Array[U32]
    // hints: " val" (U32), " ref" (inner Array), " ref" (outer Array)
    let nested: Array[Array[U32]] = Array[Array[U32]]
    // hints: " val" (U32), " ref" (outer Array; inner ref explicit)
    let partial: Array[Array[U32] ref] = Array[Array[U32]]
    // hint: " val" (U32 only; other caps explicit)
    let full: Array[Array[U32] ref] ref = Array[Array[U32]]
    // hint: ": Array[U32 val] ref" (no annotation, full inferred type)
    let inferred = Array[U32]
    // hint: " ref" (inner U32 val explicit, outer Array cap inferred)
    let primitive_partial: Array[U32 val] = Array[U32]
    // hints: " val" (U32 on next line), " ref" (Array after ] below)
    let multiline_arr: Array[
      U32
    ] = Array[U32]
    arr_u32.size() + nested.size() + partial.size() + full.size() +
      inferred.size() + primitive_partial.size() + multiline_arr.size()

class _GenericFields
  """
  Tests for inlay hints on class fields and function return type annotations.
  """
  // hints: " val" (U32), " ref" (Array)
  let items: Array[U32]
  // hints: " val" (U32), " ref" (inner Array), " ref" (outer Array)
  var nested: Array[Array[U32]]
  // hints: " val" (U32), " ref" (Array)
  embed embedded: Array[U32]

  new create() =>
    items = Array[U32]
    nested = Array[Array[U32]]
    embedded = Array[U32]

  // hint: " val" (U32), " ref" (Array)
  fun make_items(): Array[U32] =>
    Array[U32]

  // hint: " val" (U32), " ref" (inner Array), " ref" (outer Array)
  fun make_nested(): Array[Array[U32]] =>
    Array[Array[U32]]
