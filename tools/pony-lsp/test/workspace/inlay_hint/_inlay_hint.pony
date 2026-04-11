class _InlayHint
  """
  Manual testing: inlay hints appear for inferred types and for explicit
  annotations that omit the capability; and after ')' when no return type
  is specified.
  """
  fun demo(): String =>
    // hint: ": String val"
    let inferred_string = "hello"
    // hint: ": Bool val"
    var inferred_bool = true
    // hint: " val" (cap inferred, after "String")
    let explicit_string: String = "world"
    // no hint (cap is explicit)
    let explicit_full: String val = "test"
    inferred_string + inferred_bool.string() + explicit_string

  // hints: " box" (implicit receiver cap), ": None val" (return type after ')')
  fun inferred_fun() =>
    "hello"

  // hints: " box" (implicit receiver cap), " val" (return type String cap)
  fun explicit_fun(): String =>
    "world"

  // hints: " box" (implicit receiver cap), " val" (return type on second line)
  fun explicit_multiline()
    : String =>
    "world"

  // no receiver cap hint (explicit ref), hint: ": None val" (return type)
  fun ref explicit_cap() =>
    None
