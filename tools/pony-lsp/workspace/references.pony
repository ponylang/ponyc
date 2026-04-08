use ".."
use "collections"
use "pony_compiler"

primitive References
  """
  Collects reference locations for a given AST node across all packages in the
  workspace, by walking every module AST and finding all nodes that resolve to
  the same definition.
  """
  fun collect(
    node: AST box,
    packages: Map[String, PackageState] box,
    include_declaration: Bool): Array[LspLocation]
  =>
    """
    Walk all module ASTs across all packages and return the source locations of
    all nodes that resolve to the same definition as `node`. Pass
    `include_declaration = false` to exclude the definition site itself.
    """
    // Literals have no referenceable identity — no references.
    let nid = node.id()
    if (nid == TokenIds.tk_true()) or (nid == TokenIds.tk_false())
      or (nid == TokenIds.tk_int()) or (nid == TokenIds.tk_float())
      or (nid == TokenIds.tk_string())
    then
      return []
    end

    // Type-literal expressions such as `None` are desugared by the compiler
    // into implicit constructor calls. The synthetic tk_newref inside a tk_call
    // at the same position has no referenceable identity — no references.
    if nid == TokenIds.tk_newref() then
      try
        let par = node.parent() as AST box
        if (par.id() == TokenIds.tk_call()) and
          (par.position() == node.position())
        then
          return []
        end
      end
    end

    // When the cursor lands on the name identifier of an entity declaration,
    // promote to the entity node so that type references (which resolve to
    // tk_class, not its tk_id child) are found by the walker.
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
    // If the node has no definitions it IS the definition.
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

    let collector = _ReferenceCollector(target)
    if not include_declaration then
      // Pre-populate the deduplication set with the declaration's key so that
      // the declaration site and any sub-nodes that produce the same span
      // are all blocked from appearing in the results.
      try
        let decl_file = target.source_file() as String val
        (let decl_start, _) = ASTIdentifier.identifier_node(target).span()
        collector.exclude(
          recover val
            decl_file + ":" +
            decl_start.line().string() + ":" +
            decl_start.column().string()
          end)
      end
    end

    for pkg_state in packages.values() do
      match pkg_state.package()
      | let pkg: Package =>
        for module in pkg.modules() do
          collector.set_file(module.file)
          module.ast.visit(collector)
        end
      end
    end
    collector.locations()

class ref _ReferenceCollector is ASTVisitor
  """
  AST visitor that collects the source locations of all nodes that resolve to a
  given target definition AST node, across all modules.
  """
  let _target: AST val
  var _current_file: String
  let _results: Array[LspLocation] ref
  let _seen: Set[String]

  new ref create(target: AST val) =>
    _target = target
    _current_file = ""
    _results = Array[LspLocation].create()
    _seen = Set[String].create()

  fun ref set_file(file: String) =>
    _current_file = file

  fun ref exclude(key: String) =>
    """
    Pre-mark a key as seen so that results at that position are suppressed.
    Used to exclude the declaration site when includeDeclaration = false.
    """
    _seen.set(key)

  fun ref visit(ast: AST box): VisitResult =>
    // Skip synthetic tk_this nodes: their definitions() resolve to the
    // enclosing class entity, causing all implicit receivers inside a
    // class to spuriously match when targeting that class.
    if ast.id() == TokenIds.tk_this() then
      return Continue
    end

    // Check if this node IS the target definition or resolves to it.
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
      let hl_node = ASTIdentifier.identifier_node(ast)
      (let start_pos, let end_pos) = hl_node.span()
      // Deduplicate: include file in key since multiple modules share
      // line numbers. Pre-excluded keys (declaration site) are in _seen.
      let key: String val =
        recover val
          _current_file + ":" + start_pos.line().string() +
          ":" + start_pos.column().string()
        end
      if not _seen.contains(key) then
        _seen.set(key)
        _results.push(
          LspLocation(
            Uris.from_path(_current_file),
            LspPositionRange(
              LspPosition.from_ast_pos(start_pos),
              LspPosition.from_ast_pos_end(end_pos))))
      end
    end
    Continue

  fun ref locations(): Array[LspLocation] =>
    _results
