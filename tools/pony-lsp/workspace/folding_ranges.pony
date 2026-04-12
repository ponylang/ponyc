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
  """
  fun collect(module: Module val): Array[JsonValue] =>
    """
    Returns an array of folding range objects for all top-level entities,
    their members, and multi-line expressions within method bodies.
    """
    let ranges: Array[JsonValue] ref = Array[JsonValue].create()
    for node in module.ast.children() do
      match node.id()
      | TokenIds.tk_class()
      | TokenIds.tk_actor()
      | TokenIds.tk_struct()
      | TokenIds.tk_primitive()
      | TokenIds.tk_trait()
      | TokenIds.tk_interface() =>
        _push_range(node, ranges)
        _collect_members(node, ranges)
      | TokenIds.tk_type() =>
        _push_range(node, ranges)
      end
    end
    ranges

  fun _collect_members(entity: AST box, ranges: Array[JsonValue] ref) =>
    """
    Walks the members node (raw child index 4) of an entity and pushes a
    folding range for each fun, be, and new member. Also collects expression-
    level ranges from within each member's body.
    """
    try
      let members = entity(4)?
      if members.id() != TokenIds.tk_members() then
        return
      end
      for member in members.children() do
        match member.id()
        | TokenIds.tk_fun()
        | TokenIds.tk_be()
        | TokenIds.tk_new() =>
          _push_range(member, ranges)
          _collect_body(member, ranges)
        end
      end
    end

  fun _collect_body(node: AST box, ranges: Array[JsonValue] ref) =>
    """
    Walks all descendants of node and pushes folding ranges for multi-line
    compound expressions: if, ifdef, while, for, repeat, match, try,
    object, lambda, barelambda, and recover blocks. tk_try_no_check covers
    desugared `with` expressions.
    """
    let collector =
      object ref is ASTVisitor
        fun ref visit(n: AST box): VisitResult =>
          match n.id()
          | TokenIds.tk_if() | TokenIds.tk_ifdef() | TokenIds.tk_while()
          | TokenIds.tk_for() | TokenIds.tk_repeat() | TokenIds.tk_match()
          | TokenIds.tk_try() | TokenIds.tk_try_no_check()
          | TokenIds.tk_object() | TokenIds.tk_lambda()
          | TokenIds.tk_barelambda() | TokenIds.tk_recover() =>
            let sl = n.line().i64() - 1
            let el = FoldingRanges._end_line(n).i64() - 1
            if el > sl then
              ranges.push(
                JsonObject.update("startLine", sl).update("endLine", el))
            end
          end
          Continue
      end
    node.visit(collector)

  fun _push_range(node: AST box, ranges: Array[JsonValue] ref) =>
    """
    Pushes a folding range for node if it spans more than one line.
    """
    let sl: I64 = node.line().i64() - 1
    let el: I64 = _end_line(node).i64() - 1
    if el > sl then
      ranges.push(JsonObject.update("startLine", sl).update("endLine", el))
    end

  fun _end_line(node: AST box): USize =>
    """
    Returns the maximum source line number among the node and all its
    descendants. Uses visit() rather than span() because span() short-
    circuits on keyword tokens (tk_class, tk_fun, etc.), returning only
    the keyword's own line and never scanning the body.
    """
    let v =
      object ref is ASTVisitor
        var _max: USize = 0
        fun ref visit(n: AST box): VisitResult =>
          let l = n.line()
          if l > _max then _max = l end
          Continue
        fun max(): USize => _max
      end
    node.visit(v)
    v.max()
