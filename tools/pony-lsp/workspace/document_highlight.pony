use ".."
use "collections"
use "pony_compiler"

primitive DocumentHighlights
  """
  Collects document highlight ranges for a given AST node within a module, by
  walking the module AST and finding all nodes that resolve to the same
  definition.
  """
  fun collect(node: AST box, module: Module val): Array[LspPositionRange] =>
    """
    Walk the module AST and return the source ranges of all nodes that resolve
    to the same definition as `node`. Includes the definition site itself.
    """
    // Determine the canonical target definition.
    // If the node has no definitions it IS the
    // definition; use it directly as the target.
    let defs = node.definitions()
    let target: AST val =
      if defs.size() > 0 then
        try
          AST(defs(0)?.raw)
        else
          return []
        end
      else
        AST(node.raw)
      end

    let collector = _HighlightCollector(target)
    module.ast.visit(collector)
    collector.highlights()

class ref _HighlightCollector is ASTVisitor
  """
  AST visitor that collects the source ranges of all nodes that resolve to a
  given target definition AST node.
  """
  let _target: AST val
  let _highlights: Array[LspPositionRange] ref
  let _seen: Set[String]

  new ref create(target: AST val) =>
    _target = target
    _highlights = Array[LspPositionRange].create()
    _seen = Set[String].create()

  fun ref visit(ast: AST box): VisitResult =>
    // Skip synthetic tk_this nodes: their definitions() resolve to the
    // enclosing class entity, causing all implicit receivers inside a
    // class to spuriously match when targeting that class.
    if ast.id() == TokenIds.tk_this() then
      return Continue
    end

    // Check if this node IS the target definition
    // or resolves to it via DefinitionResolver
    var matches = (ast == _target)
    if not matches then
      for def in ast.definitions().values() do
        if def == _target then
          matches = true
          break
        end
      end
    end

    if matches then
      let hl_node = _node_to_highlight(ast)
      (let start_pos, let end_pos) = hl_node.span()
      // Deduplicate: skip if this start position was already added
      // (e.g. declaration node and its tk_id child can both resolve
      // to the same target and produce the same highlight span).
      let key: String val =
        recover val
          start_pos.line().string() + ":" + start_pos.column().string()
        end
      if not _seen.contains(key) then
        _seen.set(key)
        _highlights.push(
          LspPositionRange(
            LspPosition.from_ast_pos(start_pos),
            LspPosition.from_ast_pos_end(end_pos)))
      end
    end
    Continue

  fun ref highlights(): Array[LspPositionRange] =>
    _highlights

  fun _node_to_highlight(ast: AST box): AST box =>
    """
    Return the sub-node whose source span should be highlighted — usually the
    identifier token.
    """
    match ast.id()
    | TokenIds.tk_class()
    | TokenIds.tk_actor()
    | TokenIds.tk_trait()
    | TokenIds.tk_interface()
    | TokenIds.tk_primitive()
    | TokenIds.tk_type()
    | TokenIds.tk_struct()
    | TokenIds.tk_flet()
    | TokenIds.tk_fvar()
    | TokenIds.tk_embed()
    | TokenIds.tk_let()
    | TokenIds.tk_var()
    | TokenIds.tk_param() =>
      // Declarations: identifier at child(0)
      try
        let id = ast(0)?
        if id.id() == TokenIds.tk_id() then
          return id
        end
      end
    | TokenIds.tk_fun()
    | TokenIds.tk_be()
    | TokenIds.tk_new()
    | TokenIds.tk_nominal()
    | TokenIds.tk_typeref() =>
      // Methods/types: identifier at child(1)
      try
        let id = ast(1)?
        if id.id() == TokenIds.tk_id() then
          return id
        end
      end
    | TokenIds.tk_fvarref()
    | TokenIds.tk_fletref()
    | TokenIds.tk_embedref() =>
      // Field references: field name identifier at child(1)
      try
        let id = ast(1)?
        if id.id() == TokenIds.tk_id() then
          return id
        end
      end
    | TokenIds.tk_funref()
    | TokenIds.tk_beref()
    | TokenIds.tk_newref()
    | TokenIds.tk_newberef()
    | TokenIds.tk_funchain()
    | TokenIds.tk_bechain() =>
      // Method call references: method name is
      // the sibling of the receiver child
      try
        let receiver = ast.child() as AST
        let method = receiver.sibling() as AST
        if method.id() == TokenIds.tk_id() then
          return method
        end
      end
    | TokenIds.tk_reference() =>
      // Variable references: identifier at child(0)
      try
        let id = ast(0)?
        if id.id() == TokenIds.tk_id() then
          return id
        end
      end
    end
    ast
