use ".."
use "collections"
use "pony_compiler"
use "json"

primitive CallHierarchy
  """
  Handles textDocument/prepareCallHierarchy, callHierarchy/incomingCalls,
  and callHierarchy/outgoingCalls.
  """

  fun prepare(node: AST box): (JsonArray | None) =>
    """
    Resolve the method at `node` and build a CallHierarchyItem array.
    Returns one item for a declaration or one item per definition for a
    polymorphic call site. Returns None if no method is found.
    """
    var found = false
    var arr = JsonArray
    for method in _methods_for_node(node).values() do
      match \exhaustive\ _build_item(method)
      | let item: JsonObject =>
        arr = arr.push(item)
        found = true
      | None => None
      end
    end
    if found then
      arr
    else
      None
    end

  fun incoming_calls(
    node: AST box,
    packages: Map[String, PackageState] box): (JsonArray | None)
  =>
    """
    Resolve the method at `node`, then cross-package walk to collect all
    call sites, grouped by their enclosing method (the callers).
    """
    match \exhaustive\ _method_for_node(node)
    | let target: AST val =>
      let collector = _IncomingCallCollector(target)
      for pkg_state in packages.values() do
        match pkg_state.package()
        | let pkg: Package =>
          for module in pkg.modules() do
            module.ast.visit(collector)
          end
        end
      end
      collector.result()
    | None => None
    end

  fun outgoing_calls(
    node: AST box,
    packages: Map[String, PackageState] box): (JsonArray | None)
  =>
    """
    Resolve the method at `node`, then walk its AST body to collect all
    methods it calls directly.
    """
    match \exhaustive\ _method_for_node(node)
    | let method: AST val =>
      let method_file =
        try
          method.source_file() as String val
        else
          ""
        end
      let collector = _OutgoingCallCollector(method_file)
      method.visit(collector)
      collector.result()
    | None => None
    end

  fun _is_synthetic_newref(ast: AST box): Bool =>
    """
    Returns true when `ast` is a synthetic tk_newref desugared from a
    type-literal expression such as `None` — identified by having a tk_call
    parent at the same source position.
    """
    if ast.id() != TokenIds.tk_newref() then
      return false
    end
    try
      let par = ast.parent() as AST box
      (par.id() == TokenIds.tk_call()) and (par.position() == ast.position())
    else
      false
    end

  fun _methods_for_node(node: AST box): Array[AST val] =>
    """
    Resolve `node` to all candidate method AST nodes. Returns:
    - [method] when the cursor is on a method node (Module._refine_node
      already promotes a tk_id method-name to the enclosing method before
      the node reaches here).
    - All method definitions when the cursor is on a call reference (for
      polymorphic dispatch one item is emitted per concrete definition).
    - Empty array when no method is found.

    The returned methods may be in a different file from `node` (e.g. when
    the cursor is on a call-ref to a method defined in another module).
    """
    let result = Array[AST val]
    let nid = node.id()

    if _is_method(nid) then
      result.push(AST(node.raw))
      return result
    end

    for def in node.definitions().values() do
      if _is_method(def.id()) then
        result.push(def)
      end
    end

    result

  fun _method_for_node(node: AST box): (AST val | None) =>
    """
    Resolve `node` to the canonical method AST node. Returns the first
    match from `_methods_for_node`, or None if no method is found.
    See `_methods_for_node` for full resolution semantics.
    """
    try
      _methods_for_node(node)(0)?
    else
      None
    end

  fun _is_method(id: TokenId): Bool =>
    (id == TokenIds.tk_fun()) or
    (id == TokenIds.tk_be()) or
    (id == TokenIds.tk_new())

  fun _is_call_ref(id: TokenId): Bool =>
    // Partial-application variants (tk_funapp, tk_beapp, tk_newapp) are
    // intentionally excluded: definitions() always returns empty for them.
    (id == TokenIds.tk_funref()) or
    (id == TokenIds.tk_funchain()) or
    (id == TokenIds.tk_beref()) or
    (id == TokenIds.tk_bechain()) or
    (id == TokenIds.tk_newref()) or
    (id == TokenIds.tk_newberef())

  fun _symbol_kind(id: TokenId): I64 =>
    if id == TokenIds.tk_new() then
      SymbolKinds.constructor()
    else
      SymbolKinds.method()
    end

  fun _entity_name(method: AST box): String =>
    """
    Return the name of the entity (class/actor/etc.) enclosing `method`,
    or an empty string if the parent chain does not resolve to an entity.
    """
    try
      // Method lives inside tk_members, which lives inside the entity node.
      // Entity: [0]=id [1]=type_params [2]=cap [3]=provides [4]=members
      let members = method.parent() as AST box
      let entity = members.parent() as AST box
      let id_node = entity(0)?
      id_node.token_value() as String
    else
      ""
    end

  fun _build_item(method: AST box): (JsonObject | None) =>
    """
    Build a LSP CallHierarchyItem JSON object for the given method AST node.
    """
    try
      // Method child layout: [0]=annotation/cap [1]=name(tk_id) ...
      let id_node = method(1)?
      if id_node.id() != TokenIds.tk_id() then
        return None
      end
      let name = id_node.token_value() as String
      let detail = _entity_name(method)
      let kind = _symbol_kind(method.id())
      let file = method.source_file() as String val
      let uri = Uris.from_path(file)

      // child 1 is tk_id — its span covers only the declared name.
      (let sel_start, let sel_end) = id_node.span()
      let sel_range =
        LspPositionRange(
          LspPosition.from_ast_pos(sel_start),
          LspPosition.from_ast_pos_end(sel_end))

      // SiblingBound prevents the range from bleeding into the next member.
      // Fall back to sel_range when there is no next sibling.
      let full_range =
        match \exhaustive\ ASTClampedRange(method, file, SiblingBound(method))
        | let r: LspPositionRange => r
        | None => sel_range
        end

      var obj =
        JsonObject
          .update("name", name)
          .update("kind", kind)
          .update("uri", uri)
          .update("range", full_range.to_json())
          .update("selectionRange", sel_range.to_json())
      if detail.size() > 0 then
        obj = obj.update("detail", detail)
      end
      obj
    else
      None
    end

class ref _IncomingCallCollector is ASTVisitor
  """
  AST visitor that collects all call sites of a target method across all
  modules, grouped by the enclosing caller method.
  """
  // FilePosition-based identity rather than pointer equality so that trait
  // default bodies grafted onto concrete types (different AST pointers,
  // same file+position) are correctly matched.
  let _target_id: FilePosition val
  // Name prefilter: definitions() is expensive (recursive type-table descent,
  // uncached). Skip it when the call-ref identifier name doesn't match the
  // target method name.
  let _target_name: String val
  // Maps caller method identity (file+position) to (method_ast, call_ranges).
  let _callers: Map[String, (AST val, Array[LspPositionRange] ref)]
  let _seen_ranges: Set[String]

  new ref create(target: AST val) =>
    let target_file =
      try
        target.source_file() as String val
      else
        ""
      end
    _target_id = FilePosition(target.position(), target_file)
    _target_name =
      try
        target(1)?.token_value() as String val
      else
        ""
      end
    _callers = Map[String, (AST val, Array[LspPositionRange] ref)].create()
    _seen_ranges = Set[String].create()

  fun ref visit(ast: AST box): VisitResult =>
    if not CallHierarchy._is_call_ref(ast.id()) then
      return Continue
    end

    if CallHierarchy._is_synthetic_newref(ast) then
      return Continue
    end

    // Name prefilter: skip definitions() when the call-ref identifier doesn't
    // match the target. ASTIdentifier.identifier_node is cheap; definitions()
    // is not.
    if _target_name.size() > 0 then
      let ident = ASTIdentifier.identifier_node(ast)
      let call_name =
        try
          ident.token_value() as String
        else
          ""
        end
      if call_name != _target_name then
        return Continue
      end
    end

    var matches = false
    for def in ast.definitions().values() do
      let def_file =
        try
          def.source_file() as String val
        else
          ""
        end
      if FilePosition(def.position(), def_file) == _target_id then
        matches = true
        break
      end
    end
    if not matches then
      return Continue
    end

    let caller =
      match \exhaustive\ _enclosing_method(ast)
      | let m: AST val => m
      | None => return Continue
      end

    // Get the call site range (position of the method name identifier).
    let hl_node = ASTIdentifier.identifier_node(ast)
    (let start_pos, let end_pos) = hl_node.span()
    let node_file =
      try
        hl_node.source_file() as String val
      else
        ""
      end
    // Start position alone is sufficient for deduplication: two syntactically
    // distinct call expressions cannot start at the same file/line/column.
    let range_key: String val =
      recover val
        String
          .> append(node_file)
          .> push(':')
          .> append(start_pos.line().string())
          .> push(':')
          .> append(start_pos.column().string())
      end
    if _seen_ranges.contains(range_key) then
      return Continue
    end
    _seen_ranges.set(range_key)

    let range =
      LspPositionRange(
        LspPosition.from_ast_pos(start_pos),
        LspPosition.from_ast_pos_end(end_pos))

    let caller_key: String val =
      recover val
        let f =
          try
            caller.source_file() as String val
          else
            ""
          end
        let pos = caller.position()
        String
          .> append(f)
          .> push(':')
          .> append(pos.line().string())
          .> push(':')
          .> append(pos.column().string())
      end
    try
      (_callers(caller_key)?)._2.push(range)
    else
      let ranges = Array[LspPositionRange].create()
      ranges.push(range)
      _callers(caller_key) = (caller, ranges)
    end

    Continue

  fun ref _enclosing_method(node: AST box): (AST val | None) =>
    // Walk up via parent() until a method node is found.
    // Returns None when parent() returns None (top of tree, no method found).
    try
      var cur = node.parent() as AST box
      while true do
        if CallHierarchy._is_method(cur.id()) then
          return AST(cur.raw)
        end
        cur = cur.parent() as AST box
      end
    end
    None

  fun ref result(): JsonArray =>
    var arr = JsonArray
    for (caller, ranges) in _callers.values() do
      match CallHierarchy._build_item(caller)
      | let item: JsonObject =>
        var from_ranges = JsonArray
        for r in ranges.values() do
          from_ranges = from_ranges.push(r.to_json())
        end
        arr =
          arr.push(
            JsonObject
              .update("from", item)
              .update("fromRanges", from_ranges))
      end
    end
    arr

class ref _OutgoingCallCollector is ASTVisitor
  """
  AST visitor that collects all methods called within a target method's body,
  grouped by callee and recording the call site ranges within the caller.

  Note: calls inside inherited trait default bodies are not visible here
  because AST.visit filters cross-source children (grafted bodies retain the
  trait's source file). This is intentional — those calls belong to the
  trait's own hierarchy, not the concrete method's.
  """
  let _method_file: String
  // Maps callee method identity (file+position) to (method_ast, call_ranges).
  let _callees: Map[String, (AST val, Array[LspPositionRange] ref)]
  let _seen_ranges: Set[String]
  // Depth of nested method declarations (object literals, lambdas). Call
  // sites inside nested methods belong to those methods' own hierarchies,
  // not the outer method's. We use visit/leave to track entry and exit so
  // that siblings after a nested declaration are still visited (returning
  // Stop would abort the entire traversal, not just the subtree).
  var _nested_depth: USize = 0

  new ref create(method_file: String) =>
    _method_file = method_file
    _callees = Map[String, (AST val, Array[LspPositionRange] ref)].create()
    _seen_ranges = Set[String].create()

  fun ref visit(ast: AST box): VisitResult =>
    if CallHierarchy._is_method(ast.id()) then
      _nested_depth = _nested_depth + 1
      return Continue
    end

    if _nested_depth > 0 then
      return Continue
    end

    if not CallHierarchy._is_call_ref(ast.id()) then
      return Continue
    end

    if CallHierarchy._is_synthetic_newref(ast) then
      return Continue
    end

    // Deduplicate by call site before iterating definitions.
    let hl_node = ASTIdentifier.identifier_node(ast)
    (let start_pos, let end_pos) = hl_node.span()
    let range_key: String val =
      recover val
        String
          .> append(_method_file)
          .> push(':')
          .> append(start_pos.line().string())
          .> push(':')
          .> append(start_pos.column().string())
      end
    if _seen_ranges.contains(range_key) then
      return Continue
    end
    _seen_ranges.set(range_key)

    let range =
      LspPositionRange(
        LspPosition.from_ast_pos(start_pos),
        LspPosition.from_ast_pos_end(end_pos))

    // Record an edge for every concrete definition (handles polymorphic
    // dispatch where one call site resolves to multiple implementations).
    for def in ast.definitions().values() do
      if CallHierarchy._is_method(def.id()) then
        let callee_key: String val =
          recover val
            let f =
              try
                def.source_file() as String val
              else
                ""
              end
            let pos = def.position()
            String
              .> append(f)
              .> push(':')
              .> append(pos.line().string())
              .> push(':')
              .> append(pos.column().string())
          end
        try
          (_callees(callee_key)?)._2.push(range)
        else
          let ranges = Array[LspPositionRange].create()
          ranges.push(range)
          _callees(callee_key) = (def, ranges)
        end
      end
    end

    Continue

  fun ref leave(ast: AST box): VisitResult =>
    if CallHierarchy._is_method(ast.id()) then
      _nested_depth = _nested_depth - 1
    end
    Continue

  fun ref result(): JsonArray =>
    var arr = JsonArray
    for (callee, ranges) in _callees.values() do
      match CallHierarchy._build_item(callee)
      | let item: JsonObject =>
        var from_ranges = JsonArray
        for r in ranges.values() do
          from_ranges = from_ranges.push(r.to_json())
        end
        arr =
          arr.push(
            JsonObject
              .update("to", item)
              .update("fromRanges", from_ranges))
      end
    end
    arr
