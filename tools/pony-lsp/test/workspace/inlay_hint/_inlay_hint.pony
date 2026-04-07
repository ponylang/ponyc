class _InlayHint
  """
  Manual testing: open this file with pony-lsp active. Inlay hints should
  appear after the variable name on inferred declarations (no explicit `:Type`),
  and should be absent on explicit declarations.

  Expected hints:
    let inferred_string = "hello"    →  ": String val"
    var inferred_bool = true         →  ": Bool"
    let inferred_array = Array[U32]  →  ": Array[U32 val] ref"

  No hint expected:
    let explicit_string: String = "world"
  """
  fun demo(): String =>
    let inferred_string = "hello"
    var inferred_bool = true
    let explicit_string: String = "world"
    let inferred_array = Array[U32]
    inferred_string + inferred_bool.string() + explicit_string +
      inferred_array.size().string()
