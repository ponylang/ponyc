use "collections"
use ast = "pony_compiler"

class ref _MaxLineVisitor is ast.ASTVisitor
  """
  Walks AST descendants to find the maximum line number reached.

  `AST.visit()` visits children, not the node itself, so callers must seed
  `max_line` with `node.line()` before calling `node.visit(visitor)`.
  """
  var max_line: USize

  new ref create(seed: USize) =>
    max_line = seed

  fun ref visit(node: ast.AST box): ast.VisitResult =>
    // Empty TK_MEMBERS has a parser-assigned position beyond the entity's
    // actual content (at the start of the next entity). Skip it so it
    // doesn't inflate end_line.
    if (node.id() == ast.TokenIds.tk_members())
      and (node.num_children() == 0)
    then
      return ast.Continue
    end
    let l = node.line()
    if l > max_line then max_line = l end
    ast.Continue

class ref _ASTDispatcher is ast.ASTVisitor
  """
  Visitor that dispatches AST nodes to matching `ASTRule` implementations.

  Builds a dispatch table from token ID to rules on construction. As it visits
  each node, it looks up matching rules, calls their `check()` method, and
  filters results through suppressions and magic comment lines.

  Also collects entity information (name, token ID, start/end line) for use by
  module-level rules like file-naming and blank-lines.
  """
  let _dispatch: Map[ast.TokenId, Array[ASTRule val] val] val
  let _source: SourceFile val
  let _magic_lines: Set[USize] val
  let _suppressions: Suppressions val
  let _diagnostics: Array[Diagnostic val]
  let _entities: Array[(String val, ast.TokenId, USize, USize)]

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
    _entities = Array[(String val, ast.TokenId, USize, USize)]
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

    // Collect entity info for file-naming and blank-lines
    if ast.TokenIds.is_entity(token_id)
      or (token_id == ast.TokenIds.tk_type())
    then
      try
        let name_node = node(0)?
        match name_node.token_value()
        | let name: String val =>
          let start_line = node.line()
          let mlv = _MaxLineVisitor(start_line)
          node.visit(mlv)
          _entities.push((name, token_id, start_line, mlv.max_line))
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
    """
    Returns collected diagnostics after AST traversal.
    """
    let result = recover iso Array[Diagnostic val] end
    for d in _diagnostics.values() do
      result.push(d)
    end
    consume result

  fun entities(): Array[(String val, ast.TokenId, USize, USize)] val =>
    """
    Returns entity info collected during traversal:
    (name, token_id, start_line, end_line).
    """
    let result =
      recover iso Array[(String val, ast.TokenId, USize, USize)] end
    for e in _entities.values() do
      result.push(e)
    end
    consume result
