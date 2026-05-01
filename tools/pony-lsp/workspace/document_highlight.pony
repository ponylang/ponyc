use ".."
use "collections"
use "pony_compiler"

type DocumentHighlight is (LspPositionRange, I64)

primitive DocumentHighlights
  """
  Collects `textDocument/documentHighlight` results for a given AST node.

  Walking the entire module AST, it finds every node that resolves to the same
  definition as the cursor node and returns each as a `(range, kind)` pair,
  where `kind` is a `DocumentHighlightKind` value (Text=1, Read=2, Write=3).
  """
  fun collect(
    node: AST box,
    module: Module val): Array[DocumentHighlight]
  =>
    """
    Walk the module AST and return the source ranges and kinds of all nodes
    that resolve to the same definition as `node`, including the definition
    site itself.

    Each element of the returned array is `(range, kind)` where `kind` is a
    `DocumentHighlightKind` value:
    - **Write (3)**: the node is the left-hand side of a `tk_assign`, or is
      any value-binding declaration (`tk_let`, `tk_var`, `tk_fvar`, `tk_flet`,
      `tk_embed`, `tk_param`). All declaration sites are Write, consistent with
      mainstream LSP convention (clangd, rust-analyzer, tsserver).
    - **Read (2)**: a reference token (field ref, var ref, param ref,
      function/behaviour/constructor call site, etc.).
    - **Text (1)**: function/method/type declarations and type references.
    """
    // Literals have no referenceable identity — do not highlight.
    let nid = node.id()
    if (nid == TokenIds.tk_true()) or (nid == TokenIds.tk_false())
      or (nid == TokenIds.tk_int()) or (nid == TokenIds.tk_float())
      or (nid == TokenIds.tk_string())
    then
      return Array[DocumentHighlight]
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
          return Array[DocumentHighlight]
        end
      end
    end

    // When the cursor lands on the name identifier of an entity declaration
    // (tk_class, tk_actor, etc.) or type parameter declaration (tk_typeparam),
    // _refine_node returns the bare tk_id rather than the enclosing node.
    // Promote to the enclosing node so that references (which resolve to
    // tk_class or tk_typeparam, not their tk_id child) are found by the walker.
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
          | TokenIds.tk_type()
          | TokenIds.tk_typeparam() =>
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
  - **Write**: the node is at child index 0 of a `tk_assign` parent, or is any
    value-binding declaration (`tk_let`, `tk_var`, `tk_fvar`, `tk_flet`,
    `tk_embed`, `tk_param`).
  - **Read**: a reference token (field ref, var ref, param ref,
    function/behaviour/constructor call site, etc.).
  - **Text**: method/function/type declarations and type references.
  """
  let _target: AST val
  let _highlights: Array[DocumentHighlight] ref
  let _seen: Set[String]

  new ref create(target: AST val) =>
    _target = target
    _highlights = Array[DocumentHighlight].create()
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
      // ponyc synthesizes a nominal self-type as the return type of
      // auto-generated constructors (tk_new) and behaviours (tk_be) in generic
      // classes and actors, forming the chain:
      // tk_new/tk_be -> tk_nominal -> tk_typeargs -> tk_typeparamref
      // This internal node resolves to the class's type param but is not a
      // user-visible occurrence — skip it.
      try
        let p = ast.parent() as AST    // tk_typeargs?
        let gp = p.parent() as AST     // tk_nominal?
        let ggp = gp.parent() as AST   // tk_new or tk_be?
        if (p.id() == TokenIds.tk_typeargs()) and
          (gp.id() == TokenIds.tk_nominal()) and
          ((ggp.id() == TokenIds.tk_new()) or (ggp.id() == TokenIds.tk_be()))
        then
          return Continue
        end
      end

      let kind: I64 =
        if _is_assign_lhs(ast) then
          // LHS of any assignment — covers regular writes,
          // local decl-with-initializer (tk_assign(tk_let/tk_var, expr)),
          // and tuple destructuring LHS (tk_assign(tk_tuple(...), ...)).
          DocumentHighlightKind.write()
        else
          _decl_or_read_kind(ast)
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

  fun _is_assign_lhs(ast: AST box): Bool =>
    """
    Returns true if `ast` is on the left-hand side of a `tk_assign`,
    including when nested inside a destructuring tuple on the LHS
    (e.g. the `a` in `(a, b) = expr`).

    Ponyc wraps each tuple element in a single-child `tk_seq` node, so
    the actual parent chain is: varref → tk_seq(1 child) → tk_tuple → tk_assign.
    We traverse through both `tk_seq` (single-child) and `tk_tuple` nodes.
    """
    try
      let par = ast.parent() as AST
      if par.id() == TokenIds.tk_assign() then
        ast.sibling_idx() == 0
      elseif par.id() == TokenIds.tk_tuple() then
        _is_assign_lhs(par)
      elseif (par.id() == TokenIds.tk_seq()) and
        (par.num_children() == 1)
      then
        _is_assign_lhs(par)
      else
        false
      end
    else
      false
    end

  fun _decl_or_read_kind(ast: AST box): I64 =>
    """
    Classify a declaration or reference node that is NOT the LHS of a
    `tk_assign`.

    - Value-binding declarations (`tk_let`, `tk_var`, `tk_fvar`, `tk_flet`,
      `tk_embed`, `tk_param`): always Write, consistent with mainstream LSP
      implementations (clangd, rust-analyzer, tsserver).
    - Reference tokens: Read.
    - Everything else (methods, types, etc.): Text.
    """
    match ast.id()
    | TokenIds.tk_let() | TokenIds.tk_var()
    | TokenIds.tk_fvar() | TokenIds.tk_flet() | TokenIds.tk_embed()
    | TokenIds.tk_param() =>
      DocumentHighlightKind.write()
    else
      _read_kind(ast.id())
    end

  fun _read_kind(id: I32): I64 =>
    """
    Map a reference token id to Read (2) or Text (1).

    Reference tokens — nodes that represent a use of a named symbol at a
    call or access site — are Read. Everything else (method/type declarations,
    type annotations) is Text.

    Note: `consume x` expressions produce a `tk_varref` or `tk_letref` for
    the consumed variable, so they are classified as Read. The LSP kind
    vocabulary has no "destructive read" concept.
    """
    match id
    | TokenIds.tk_fvarref()
    | TokenIds.tk_fletref()
    | TokenIds.tk_embedref()
    | TokenIds.tk_tupleelemref()
    | TokenIds.tk_varref()
    | TokenIds.tk_letref()
    | TokenIds.tk_paramref()
    | TokenIds.tk_funref()
    | TokenIds.tk_funapp()
    | TokenIds.tk_beref()
    | TokenIds.tk_beapp()
    | TokenIds.tk_newref()
    | TokenIds.tk_newberef()
    | TokenIds.tk_newapp()
    | TokenIds.tk_funchain()
    | TokenIds.tk_bechain()
    | TokenIds.tk_reference() =>
      DocumentHighlightKind.read()
    else
      DocumentHighlightKind.text()
    end

  fun ref highlights(): Array[DocumentHighlight] =>
    _highlights
