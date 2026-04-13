use ".."
use "json"
use "pony_compiler"

primitive FoldingRanges
  """
  Collects textDocument/foldingRange results for a module.

  Returns one FoldingRange per top-level type entity (class, actor, struct,
  trait, interface, primitive, type alias) and one per member (fun, be, new).
  Also returns ranges for multi-line compound expressions within method bodies
  (if, ifdef, while, for, repeat, match, try, object, lambda, recover).
  Single-line nodes are skipped since there is nothing to fold.

  Note: `with` is desugared to `tk_try_no_check` before typechecking, so
  `tk_with` never appears in the AST that the LSP processes. Fold ranges for
  `with` expressions are produced via the `tk_try_no_check` arm.

  End-line calculation uses a `cap` equal to the next sibling's start line.
  This prevents synthetic nodes introduced by desugaring (e.g. the constructor
  call injected by `with` sugar) from inflating a fold range past the entity
  or member boundary where it logically ends.

  Synthetic entity detection relies on source-line monotonicity: user-defined
  entities appear in `module.ast.children()` in ascending source-line order,
  while compiler-generated entities (desugared `object`/lambda classes,
  dispatch helpers, etc.) may appear non-monotonically. Any entity whose line
  does not strictly exceed the running maximum is treated as synthetic.

  Known limitation: this heuristic misclassifies a desugared `object` or
  lambda class as a real entity when it appears in a file where the entity
  containing the expression is the last entity by start line, because the
  synthetic class's source position (inside the entity body) exceeds the
  entity's start line and passes the monotonicity check. In practice this
  only affects folding ranges for files where either: (a) there is exactly
  one top-level entity and it contains an `object`/lambda literal, or (b)
  the last top-level entity by start line contains an `object`/lambda and
  there is no subsequent entity with a higher start line to act as a sentinel.
  """
  fun collect(module: Module val): Array[JsonValue] =>
    """
    Returns an array of folding range objects for all top-level entities,
    their members, and multi-line expressions within method bodies.
    """
    let ranges: Array[JsonValue] ref = Array[JsonValue].create()
    let entities: Array[AST box] = Array[AST box]
    let synthetic_entities: Array[AST box] = Array[AST box]
    var max_entity_line: USize = 0
    for node in module.ast.children() do
      match node.id()
      | TokenIds.tk_class()
      | TokenIds.tk_actor()
      | TokenIds.tk_struct()
      | TokenIds.tk_primitive()
      | TokenIds.tk_trait()
      | TokenIds.tk_interface()
      | TokenIds.tk_type() =>
        let node_line = node.line()
        if node_line > max_entity_line then
          entities.push(node)
          max_entity_line = node_line
        else
          synthetic_entities.push(node)
        end
      end
    end
    for i in entities.keys() do
      try
        let node = entities(i)?
        let cap =
          try entities(i + 1)?.line()
          else USize.max_value()
          end
        match node.id()
        | TokenIds.tk_class()
        | TokenIds.tk_actor()
        | TokenIds.tk_struct()
        | TokenIds.tk_primitive()
        | TokenIds.tk_trait()
        | TokenIds.tk_interface() =>
          _push_range(node, ranges, cap)
          _collect_members(node, ranges, cap)
        | TokenIds.tk_type() =>
          _push_range(node, ranges, cap)
        end
      end
    end
    for node in synthetic_entities.values() do
      _push_range(node, ranges)
    end
    ranges

  fun _collect_members(
    entity: AST box,
    ranges: Array[JsonValue] ref,
    entity_cap: USize)
  =>
    """
    Walks the members node (raw child index 4) of an entity and pushes a
    folding range for each fun, be, and new member. Also collects expression-
    level ranges from within each member's body.

    Members are filtered to exclude synthetic nodes: we skip any member whose
    line equals the entity's line (synthetic default constructors injected by
    ponyc) and any member whose body does not extend past its declaration line
    (synthetic single-line methods such as eq, ne, hash added for primitives).
    """
    try
      let members = entity(4)?
      if members.id() != TokenIds.tk_members() then
        return
      end
      let entity_line = entity.line()
      let real_members: Array[AST box] = Array[AST box]
      for member in members.children() do
        match member.id()
        | TokenIds.tk_fun()
        | TokenIds.tk_be()
        | TokenIds.tk_new() =>
          if (member.line() > entity_line) and
            (_end_line(member, entity_cap) > member.line())
          then
            real_members.push(member)
          end
        end
      end
      for i in real_members.keys() do
        let member = real_members(i)?
        let member_cap =
          try real_members(i + 1)?.line()
          else entity_cap
          end
        _push_range(member, ranges, member_cap)
        _collect_body(member, ranges, member_cap)
      end
    end

  fun _collect_body(
    node: AST box,
    ranges: Array[JsonValue] ref,
    cap: USize)
  =>
    """
    Walks all descendants of node and pushes folding ranges for multi-line
    compound expressions: if, ifdef, while, for, repeat, match, try, and
    recover blocks. tk_try_no_check covers desugared `with` expressions.

    Note: tk_object, tk_lambda, and tk_barelambda are not matched here
    because PassExpr replaces those literals with constructor calls before
    the AST reaches the LSP. Fold ranges for object/lambda expressions are
    produced via the synthetic entity classes appended to the module.
    """
    let collector =
      object ref is ASTVisitor
        fun ref visit(n: AST box): VisitResult =>
          match n.id()
          | TokenIds.tk_if() | TokenIds.tk_ifdef() | TokenIds.tk_while()
          | TokenIds.tk_for() | TokenIds.tk_repeat() | TokenIds.tk_match()
          | TokenIds.tk_try() | TokenIds.tk_try_no_check()
          | TokenIds.tk_recover() =>
            let sl = n.line().i64() - 1
            let el = FoldingRanges._end_line(n, cap).i64() - 1
            if el > sl then
              ranges.push(
                JsonObject.update("startLine", sl).update("endLine", el))
            end
          end
          Continue
      end
    node.visit(collector)

  fun _push_range(
    node: AST box,
    ranges: Array[JsonValue] ref,
    cap: USize = USize.max_value())
  =>
    """
    Pushes a folding range for node if it spans more than one line.
    """
    let sl: I64 = node.line().i64() - 1
    let el: I64 = _end_line(node, cap).i64() - 1
    if el > sl then
      ranges.push(JsonObject.update("startLine", sl).update("endLine", el))
    end

  fun _end_line(node: AST box, cap: USize = USize.max_value()): USize =>
    """
    Returns the maximum source line number (1-indexed) among all descendants
    of node that fall before `cap`. The cap is the start line of the
    next sibling entity or member, preventing synthetic nodes introduced by
    desugaring from inflating the range past the logical boundary.

    Uses visit() rather than span() because span() short-circuits on keyword
    tokens (tk_class, tk_fun, etc.), returning only the keyword's own line and
    never scanning the body.
    """
    let v =
      object ref is ASTVisitor
        var _max: USize = 0
        fun ref visit(n: AST box): VisitResult =>
          let l = n.line()
          if (l > _max) and (l < cap) then _max = l end
          Continue
        fun max(): USize => _max
      end
    node.visit(v)
    v.max()
