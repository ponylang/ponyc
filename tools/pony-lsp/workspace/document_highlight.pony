use ".."
use "collections"
use "pony_compiler"

primitive DocumentHighlights
  """
  Collects `textDocument/documentHighlight` results for a given AST node.

  Walking the entire module AST, it finds every node that resolves to the same
  definition as the cursor node and returns each as a `(range, kind)` pair,
  where `kind` is a `DocumentHighlightKind` value (Text=1, Read=2, Write=3).
  """
  fun collect(
    node: AST box,
    module: Module val): Array[(LspPositionRange, I64)]
  =>
    """
    Walk the module AST and return the source ranges and kinds of all nodes
    that resolve to the same definition as `node`, including the definition
    site itself.

    Each element of the returned array is `(range, kind)` where `kind` is a
    `DocumentHighlightKind` value:
    - **Write (3)**: the node is the left-hand side of a `tk_assign` (but not
      a local declaration-with-initializer, which ponyc also represents as
      `tk_assign(tk_let/tk_var, expr)`).
    - **Read (2)**: the node is a reference token (field ref, var ref, param
      ref, function/behaviour/constructor call site, etc.).
    - **Text (1)**: everything else — declarations and type references.
    """
    // Literals have no referenceable identity — do not highlight.
    let nid = node.id()
    if (nid == TokenIds.tk_true()) or (nid == TokenIds.tk_false())
      or (nid == TokenIds.tk_int()) or (nid == TokenIds.tk_float())
      or (nid == TokenIds.tk_string())
    then
      return Array[(LspPositionRange, I64)]
    end

    // Type-literal expressions such as `None` are desugared by the compiler
    // into implicit constructor calls `TypeName.create()`. The resulting
    // tk_newref and its enclosing tk_call are both placed at the same source
    // position (the marker ponyc uses for synthetic, position-less nodes).
    // These have no referenceable identity the user can navigate — do not
    // highlight them.
    if nid == TokenIds.tk_newref() then
      try
        let par = node.parent() as AST box
        if (par.id() == TokenIds.tk_call()) and
          (par.position() == node.position())
        then
          return Array[(LspPositionRange, I64)]
        end
      end
    end

    // When the cursor lands on the name identifier of an entity declaration
    // (tk_class, tk_actor, etc.), _refine_node returns the bare tk_id rather
    // than the enclosing entity node. Promote to the entity node so that
    // references to the type (which resolve to tk_class, not its tk_id child)
    // are found by the walker.
    let node' =
      if nid == TokenIds.tk_id() then
        try
          let par = node.parent() as AST box
          match par.id()
          | TokenIds.tk_class()
          | TokenIds.tk_actor()
          | TokenIds.tk_struct()
          | TokenIds.tk_primitive()
          | TokenIds.tk_trait()
          | TokenIds.tk_interface()
          | TokenIds.tk_type() =>
            par
          else
            node
          end
        else
          node
        end
      else
        node
      end

    // Determine the canonical target definition.
    // If the node has no definitions it IS the
    // definition; use it directly as the target.
    let defs = node'.definitions()
    let target: AST val =
      if defs.size() > 0 then
        try
          AST(defs(0)?.raw)
        else
          return []
        end
      else
        AST(node'.raw)
      end

    let collector = _HighlightCollector(target)
    module.ast.visit(collector)
    collector.highlights()

class ref _HighlightCollector is ASTVisitor
  """
  AST visitor that collects `(range, kind)` pairs for every node in the module
  that resolves to a given target definition.

  Kind assignment rules (evaluated on the matched AST node before extracting
  its identifier sub-node for the span):
  - **Write**: the node is at child index 0 of a `tk_assign` parent, and is
    not itself a declaration node (`tk_let`/`tk_var` with initializer).
  - **Read**: the node's token id is one of the reference kinds listed in
    `_read_kind`.
  - **Text**: all other nodes — declarations (`tk_fvar`, `tk_fun`, `tk_class`,
    etc.) and type references (`tk_nominal`, `tk_typeref`).
  """
  let _target: AST val
  let _highlights: Array[(LspPositionRange, I64)] ref
  let _seen: Set[String]

  new ref create(target: AST val) =>
    _target = target
    _highlights = Array[(LspPositionRange, I64)].create()
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
      let kind: I64 =
        try
          let par = ast.parent() as AST
          // Local declarations with initializers (let x = v, var x = v) are
          // represented as tk_assign(tk_let/tk_var, expr) in the AST. These
          // are declaration sites, not writes — exclude them from Write kind.
          let is_decl =
            (ast.id() == TokenIds.tk_let()) or (ast.id() == TokenIds.tk_var())
          if (par.id() == TokenIds.tk_assign()) and (ast.sibling_idx() == 0)
            and not is_decl
          then
            DocumentHighlightKind.write()
          else
            _read_kind(ast.id())
          end
        else
          _read_kind(ast.id())
        end
      let hl_node = ASTIdentifier.identifier_node(ast)
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
          ( LspPositionRange(
              LspPosition.from_ast_pos(start_pos),
              LspPosition.from_ast_pos_end(end_pos))
          , kind))
      end
    end
    Continue

  fun _read_kind(id: I32): I64 =>
    """
    Map a token id to Read (2) or Text (1).

    Reference tokens — nodes that represent a use of a named symbol rather
    than its declaration or its appearance in a type expression — are Read.
    Everything else (declarations, type annotations) is Text.
    """
    match id
    | TokenIds.tk_fvarref()
    | TokenIds.tk_fletref()
    | TokenIds.tk_embedref()
    | TokenIds.tk_varref()
    | TokenIds.tk_letref()
    | TokenIds.tk_paramref()
    | TokenIds.tk_funref()
    | TokenIds.tk_beref()
    | TokenIds.tk_newref()
    | TokenIds.tk_newberef()
    | TokenIds.tk_funchain()
    | TokenIds.tk_bechain()
    | TokenIds.tk_reference() =>
      DocumentHighlightKind.read()
    else
      DocumentHighlightKind.text()
    end

  fun ref highlights(): Array[(LspPositionRange, I64)] =>
    _highlights
