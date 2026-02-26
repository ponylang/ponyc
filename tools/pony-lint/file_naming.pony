use ast = "pony_compiler"

primitive FileNaming is ASTRule
  """
  Flags source files whose name does not match the principal type defined
  within.

  The principal type is determined by heuristic:
  1. Single entity — that is the principal type.
  2. Trait or interface present with other subordinate types — the
     trait/interface is the principal type.
  3. Otherwise — no principal type; the rule does not flag the file.

  Exception: `_test.pony` files with a `Main` actor are never flagged. This is
  the standard Pony convention for library test runner packages.

  The expected filename is the principal type name converted to snake_case,
  plus `.pony`. Private types (leading `_`) produce filenames with the leading
  underscore preserved: `_MyType` becomes `_my_type.pony`.

  This rule operates at the module level. The dispatcher collects entity
  information during its walk, and `check_module()` is called once per module
  after traversal.
  """
  fun id(): String val => "style/file-naming"
  fun category(): String val => "style"
  fun description(): String val =>
    "file name should match principal type"
  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    // Not used for per-node dispatch — this rule uses check_module
    recover val Array[ast.TokenId] end

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    // Per-node check is a no-op; see check_module
    recover val Array[Diagnostic val] end

  fun check_module(
    entities: Array[(String val, ast.TokenId, USize)] val,
    source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check that the filename matches the principal type in the module.
    `entities` is an array of (name, token_id, line_count) tuples collected
    during AST traversal.
    """
    if entities.size() == 0 then
      return recover val Array[Diagnostic val] end
    end

    let principal =
      match _find_principal(entities)
      | let name: String val => name
      else
        return recover val Array[Diagnostic val] end
      end

    let expected = NamingHelpers.to_snake_case(principal)
    let actual = _filename_stem(source.path)

    // _test.pony with a Main actor is the standard Pony test runner
    // convention — not a naming violation.
    if (actual == "_test") and (principal == "Main") then
      return recover val Array[Diagnostic val] end
    end

    if expected != actual then
      recover val
        [Diagnostic(id(),
          "file '" + _filename_stem(source.path) + ".pony' should be named '"
            + expected + ".pony' (principal type: " + principal + ")",
          source.rel_path, 1, 1)]
      end
    else
      recover val Array[Diagnostic val] end
    end

  fun _find_principal(
    entities: Array[(String val, ast.TokenId, USize)] val)
    : (String val | None)
  =>
    """
    Determine the principal type from the entities in a module.
    """
    if entities.size() == 0 then return None end

    // 1. Single entity
    if entities.size() == 1 then
      try return entities(0)?._1 end
    end

    // 2. Trait/interface with subordinates
    var trait_name: (String val | None) = None
    var non_trait_count: USize = 0
    for (name, token_id, _) in entities.values() do
      if (token_id == ast.TokenIds.tk_trait())
        or (token_id == ast.TokenIds.tk_interface())
      then
        trait_name = name
      else
        non_trait_count = non_trait_count + 1
      end
    end
    if (trait_name isnt None) and (non_trait_count > 0) then
      return trait_name
    end

    // 3. No clear principal — skip
    None

  fun _filename_stem(path: String val): String val =>
    """Extract filename without .pony extension from a full path."""
    var base = path
    // Find last path separator
    var i = path.size()
    while i > 0 do
      i = i - 1
      try
        let ch = path(i)?
        if ch == '/' then
          base = path.substring((i + 1).isize())
          break
        end
      end
    end
    // Strip .pony extension
    if base.at(".pony", (base.size() - 5).isize()) then
      base.substring(0, (base.size() - 5).isize())
    else
      base
    end
