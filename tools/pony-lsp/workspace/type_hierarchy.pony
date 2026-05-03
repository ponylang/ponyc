use ".."
use "collections"
use "pony_compiler"
use "json"

primitive TypeHierarchy
  """
  Handles textDocument/prepareTypeHierarchy, typeHierarchy/supertypes,
  and typeHierarchy/subtypes.
  """

  fun prepare(node: AST box): (JsonArray | None) =>
    """
    Resolve the entity at `node` and build a one-element TypeHierarchyItem
    array, or None if no entity is found at the cursor.
    """
    match \exhaustive\ _entity_for_node(node)
    | let entity: AST val =>
      match \exhaustive\ _build_item(entity)
      | let item: JsonObject => JsonArray.push(item)
      | None => None
      end
    | None => None
    end

  fun supertypes(node: AST box): (JsonArray | None) =>
    """
    Resolve the entity at `node` and return TypeHierarchyItems for each
    type in its provides list (direct supertypes only).
    """
    match \exhaustive\ _entity_for_node(node)
    | let entity: AST val => _collect_supertypes(entity)
    | None => None
    end

  fun subtypes(
    node: AST box,
    packages: Map[String, PackageState] box): (JsonArray | None)
  =>
    """
    Resolve the entity at `node`, then cross-package walk to collect all
    entities whose provides list directly references that entity.
    """
    match \exhaustive\ _entity_for_node(node)
    | let entity: AST val =>
      let collector = _SubtypeCollector(entity)
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

  fun _entity_for_node(node: AST box): (AST val | None) =>
    """
    Resolve `node` to a canonical entity AST node, or None if the node does
    not sit on an entity declaration.
    """
    match \exhaustive\ _ResolveASTTarget(node)
    | let resolved: AST val =>
      if _is_entity(resolved.id()) then
        resolved
      else
        None
      end
    | None => None
    end

  fun _is_entity(id: TokenId): Bool =>
    // Delegate to the compiler library so new entity tokens are picked up
    // automatically. tk_object (anonymous object literals) is excluded: they
    // have no declared name and aren't meaningful as hierarchy nodes.
    TokenIds.is_entity(id) and (id != TokenIds.tk_object())

  fun _symbol_kind(id: TokenId): I64 =>
    match id
    | TokenIds.tk_interface()
    | TokenIds.tk_trait() =>
      SymbolKinds.sk_interface()
    | TokenIds.tk_struct() =>
      SymbolKinds.sk_struct()
    else
      // LSP has no dedicated kind for actor or primitive; sk_class is the
      // nearest semantic equivalent.
      SymbolKinds.sk_class()
    end

  fun _build_item(entity: AST box): (JsonObject | None) =>
    """
    Build a LSP TypeHierarchyItem JSON object for the given entity AST node.
    """
    try
      let id_node = entity(0)?
      if id_node.id() != TokenIds.tk_id() then
        return None
      end
      let name = id_node.token_value() as String
      let kind = _symbol_kind(entity.id())
      let file = entity.source_file() as String val
      let uri = Uris.from_path(file)

      // child 0 is tk_id — its span covers only the declared name, which is
      // what editors highlight when the user selects this item.
      (let sel_start, let sel_end) = id_node.span()
      let sel_range =
        LspPositionRange(
          LspPosition.from_ast_pos(sel_start),
          LspPosition.from_ast_pos_end(sel_end))

      // SiblingBound prevents range from bleeding into the next entity.
      // Fall back to sel_range when ASTClampedRange returns None (last entity
      // in file with no sibling to cap against).
      let full_range =
        match \exhaustive\ ASTClampedRange(entity, file, SiblingBound(entity))
        | let r: LspPositionRange => r
        | None => sel_range
        end

      JsonObject
        .update("name", name)
        .update("kind", kind)
        .update("uri", uri)
        .update("range", full_range.to_json())
        .update("selectionRange", sel_range.to_json())
    else
      None
    end

  fun _collect_supertypes(entity: AST box): JsonArray =>
    """
    Walk the entity's provides node and build TypeHierarchyItems for each
    directly referenced supertype.
    """
    var result = JsonArray
    let seen = Set[AST val]
    try
      // Entity child layout:
      // [0]=id [1]=type_params [2]=cap [3]=provides [4]=members
      let provides_node = entity(3)?
      if provides_node.id() == TokenIds.tk_none() then
        return result
      end
      for def in _provides_defs(provides_node).values() do
        // Dedup by AST pointer identity — same invariant as _SubtypeCollector.
        if seen.contains(def) then
          continue
        end
        seen.set(def)
        match _build_item(def)
        | let item: JsonObject => result = result.push(item)
        end
      end
    end
    result

  fun _provides_mentions(provides: AST box, name: String): Bool =>
    """
    Return true if any nominal in the provides subtree has an identifier
    matching `name`. Cheap identifier-only walk; does not call definitions().
    """
    let nid = provides.id()
    if nid == TokenIds.tk_none() then
      false
    elseif
      (nid == TokenIds.tk_provides()) or (nid == TokenIds.tk_isecttype())
    then
      for child in provides.children() do
        if _provides_mentions(child, name) then
          return true
        end
      end
      false
    else
      try
        (ASTIdentifier.identifier_node(provides).token_value() as String)
          == name
      else
        true  // non-nominal type form — allow through
      end
    end

  fun _provides_defs(provides: AST box): Array[AST val] =>
    """
    Return all entity definitions directly referenced by a provides node.
    Recursively descends through tk_provides and tk_isecttype wrappers
    until reaching resolvable type nodes (e.g. tk_nominal).
    """
    let result = Array[AST val]
    _append_provides_defs(provides, result)
    result

  fun _append_provides_defs(provides: AST box, result: Array[AST val] ref) =>
    // Valid provides shapes: tk_none (empty), tk_provides (wrapper around the
    // provides type), tk_isecttype (intersection &). The parser rejects | at
    // the top of a provides clause, so tk_uniontype never appears here.
    // Anything else is a nominal; resolve it directly via definitions().
    let nid = provides.id()
    if nid == TokenIds.tk_none() then
      None
    elseif (nid == TokenIds.tk_provides())
      or (nid == TokenIds.tk_isecttype()) then
      for child in provides.children() do
        _append_provides_defs(child, result)
      end
    else
      for def in provides.definitions().values() do
        if _is_entity(def.id()) then
          result.push(def)
        end
      end
    end

class ref _SubtypeCollector is ASTVisitor
  """
  AST visitor that collects entities whose provides list directly references
  the target entity.
  """
  let _target: AST val
  // Name prefilter: _provides_defs calls definitions() which is an expensive
  // recursive type-table descent. Skip it when no nominal in the provides
  // clause even mentions the target name. Mirrors _IncomingCallCollector.
  let _target_name: String val
  var _result: JsonArray

  new ref create(target: AST val) =>
    _target = target
    _target_name =
      try
        target(0)?.token_value() as String val
      else
        ""
      end
    _result = JsonArray

  fun ref visit(ast: AST box): VisitResult =>
    if not TypeHierarchy._is_entity(ast.id()) then
      return Continue
    end
    try
      // Entity child layout:
      // [0]=id [1]=type_params [2]=cap [3]=provides [4]=members
      let provides_node = ast(3)?
      if provides_node.id() == TokenIds.tk_none() then
        return Continue
      end
      // Name prefilter: cheap identifier-only walk before the expensive
      // definitions() call inside _provides_defs.
      if (_target_name.size() > 0)
        and not TypeHierarchy._provides_mentions(provides_node, _target_name)
      then
        return Continue
      end
      for def in TypeHierarchy._provides_defs(provides_node).values() do
        // Pointer identity is valid here: within a single compile graph each
        // entity has a unique AST node, so == is an exact match.
        if def == _target then
          match TypeHierarchy._build_item(ast)
          | let item: JsonObject => _result = _result.push(item)
          end
          return Continue
        end
      end
    end
    Continue

  fun ref result(): JsonArray =>
    _result
