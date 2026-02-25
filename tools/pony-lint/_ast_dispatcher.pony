use "collections"
use ast = "pony_compiler"

class ref _ASTDispatcher is ast.ASTVisitor
  """
  Visitor that dispatches AST nodes to matching `ASTRule` implementations.

  Builds a dispatch table from token ID to rules on construction. As it visits
  each node, it looks up matching rules, calls their `check()` method, and
  filters results through suppressions and magic comment lines.

  Also collects entity information (name, token ID, line span) for use by
  module-level rules like file-naming.
  """
  let _dispatch: Map[ast.TokenId, Array[ASTRule val] val] val
  let _source: SourceFile val
  let _magic_lines: Set[USize] val
  let _suppressions: Suppressions val
  let _diagnostics: Array[Diagnostic val]
  let _entities: Array[(String val, ast.TokenId, USize)]

  new ref create(
    rules: Array[ASTRule val] val,
    source: SourceFile val,
    magic_lines: Set[USize] val,
    suppressions: Suppressions val)
  =>
    _source = source
    _magic_lines = magic_lines
    _suppressions = suppressions
    _diagnostics = Array[Diagnostic val]
    _entities = Array[(String val, ast.TokenId, USize)]
    _dispatch = _build_dispatch(rules)

  fun tag _build_dispatch(rules: Array[ASTRule val] val)
    : Map[ast.TokenId, Array[ASTRule val] val] val
  =>
    // First pass: group rules by token ID into a temporary ref map
    let table = Map[ast.TokenId, Array[ASTRule val]]
    for rule in rules.values() do
      for token_id in rule.node_filter().values() do
        let bucket =
          try
            table(token_id)?
          else
            let b = Array[ASTRule val]
            table(token_id) = b
            b
          end
        bucket.push(rule)
      end
    end

    // Second pass: convert to val map with val arrays
    let result = recover iso Map[ast.TokenId, Array[ASTRule val] val] end
    for (k, v) in table.pairs() do
      let arr = recover iso Array[ASTRule val](v.size()) end
      for r in v.values() do
        arr.push(r)
      end
      result(k) = consume arr
    end
    consume result

  fun ref visit(node: ast.AST box): ast.VisitResult =>
    let token_id = node.id()

    // Collect entity info for file-naming
    if ast.TokenIds.is_entity(token_id)
      or (token_id == ast.TokenIds.tk_type())
    then
      try
        let name_node = node(0)?
        match name_node.token_value()
        | let name: String val =>
          (let start_pos, let end_pos) = node.span()
          let line_count =
            if end_pos.line() >= start_pos.line() then
              (end_pos.line() - start_pos.line()) + 1
            else
              1
            end
          _entities.push((name, token_id, line_count))
        end
      end
    end

    // Dispatch to matching rules
    try
      let rules = _dispatch(token_id)?
      for rule in rules.values() do
        let diags = rule.check(node, _source)
        for diag in diags.values() do
          if _magic_lines.contains(diag.line) then continue end
          if _suppressions.is_suppressed(diag.line, diag.rule_id) then
            continue
          end
          _diagnostics.push(diag)
        end
      end
    end
    ast.Continue

  fun diagnostics(): Array[Diagnostic val] val =>
    """Returns collected diagnostics after AST traversal."""
    let result = recover iso Array[Diagnostic val] end
    for d in _diagnostics.values() do
      result.push(d)
    end
    consume result

  fun entities(): Array[(String val, ast.TokenId, USize)] val =>
    """
    Returns entity info collected during traversal:
    (name, token_id, line_count).
    """
    let result = recover iso Array[(String val, ast.TokenId, USize)] end
    for e in _entities.values() do
      result.push(e)
    end
    consume result
