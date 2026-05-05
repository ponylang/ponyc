use "pony_compiler"

primitive ASTSourceSpan
  """
  Computes the true source span of an AST node, including all descendants
  whose `source_file()` is None (synthetic container nodes) or matches
  `doc_path` (leaf tokens from the current file). Descendants whose
  source_file() is a non-empty string that does not match doc_path are
  excluded — this filters trait method bodies merged from other files.

  The optional `max_pos` parameter caps the end of the computed span:
  only descendants whose end position is strictly less than `max_pos`
  contribute to `_max`. Pass the start position of the next sibling
  entity to prevent synthesized constructors (ponyc inserts them for
  bare primitives, bare classes, etc.) from inflating the entity's range
  into the next entity's lines.

  A second guard caps the final `_max` column to the actual length of
  that source line. Synthesized constructors for entities with explicit
  members and desugared value expressions (e.g. `None => None`) can
  produce `end_pos()` values past the actual line end. Reading the
  source text caps the column to a position that exists in the file.

  Seeded with the node's own position and end_pos (if any) so that
  declaration keywords (tk_fun, tk_class, etc.) are included even though
  they are the node's token rather than a child.

  Note: AST.span() short-circuits on keyword tokens returning only the
  keyword extent; this helper always scans children to get the full body
  range. Required by any LSP response that quotes a "full declaration
  range" for entity or member declarations.

  Returns None when the computed span is inverted (should not occur for
  well-formed AST nodes).
  """

  fun tag apply(
    n: AST box,
    doc_path: String val,
    max_pos: (Position | None) = None)
    : ((Position, Position) | None)
  =>
    """
    Compute the span of `n`, returning `(start, end)` in ponyc's native
    `Position` units (1-based line, 1-based column, end inclusive), or
    `None` if the computed span is inverted.
    """
    // Seed with this node's own extent. Declaration nodes like tk_fun and
    // tk_class have end_pos() set to their keyword's last column — include
    // that so the keyword itself is covered even if no children are visited.
    let n_start = n.position()
    let n_end =
      match \exhaustive\ n.end_pos()
      | let ep: Position => ep
      | None => n_start
      end

    let visitor =
      object ref is ASTVisitor
        var _min: Position = n_start
        var _max: Position = n_end

        fun ref visit(child: AST box): VisitResult =>
          // Exclude descendants from other source files. This is the
          // primary filter for nodes that reach here via a synthetic
          // (None source_file) intermediary: AST.visit()'s own
          // _from_same_source() returns true when either side is None,
          // so a chain our_file -> None-source -> other_file can pass
          // AST.visit()'s check. The explicit test here catches that.
          // Note: returning Continue still descends into the child's
          // subtree — only the child's own position is excluded from
          // the min/max accumulation, not its descendants.
          match child.source_file()
          | let sf: String val =>
            if sf != doc_path then
              return Continue
            end
          end
          let pos = child.position()
          if pos < _min then
            _min = pos
          end
          let child_ep =
            match \exhaustive\ child.end_pos()
            | let ep: Position => ep
            | None => pos
            end
          // Only extend _max if child_ep is within the allowed bound.
          // This prevents synthesized constructors (whose body tokens
          // ponyc positions at the start of the next sibling entity)
          // from inflating the span past the current entity's content.
          let ep_in_bounds =
            match \exhaustive\ max_pos
            | None => true
            | let m: Position => child_ep < m
            end
          if ep_in_bounds and (child_ep > _max) then
            _max = child_ep
          end
          Continue

        fun ref leave(child: AST box): VisitResult =>
          Continue

        fun min(): Position =>
          _min

        fun max(): Position =>
          _max
      end
    n.visit(visitor)

    let min = visitor.min()
    let max =
      match \exhaustive\ n.source_contents()
      | let src: String box => _cap_to_line_length(src, visitor.max())
      | None => visitor.max()
      end
    if min <= max then
      (min, max)
    else
      None
    end

  fun tag _cap_to_line_length(src: String box, pos: Position): Position =>
    """
    Cap pos.column() to the number of characters on that line in src.
    Guards against synthesized-node end_pos() overshoots past line end.
    """
    if pos.line() == 0 then
      return pos
    end
    try
      let tgt = pos.line()
      let line_len: USize =
        if tgt <= 1 then
          try
            src.find("\n")?.usize()
          else
            src.size()
          end
        else
          // nth=k finds the (k+1)th newline (0-indexed)
          let prev_nl = src.find("\n" where nth = (tgt - 2))?.usize()
          let ls = prev_nl + 1
          try
            src.find("\n" where nth = (tgt - 1))?.usize() - ls
          else
            src.size() - ls
          end
        end
      if (line_len > 0) and (pos.column() > line_len) then
        Position(tgt, line_len)
      else
        pos
      end
    else
      pos
    end

primitive ASTClampedRange
  """
  Computes a clamped `LspPositionRange` from an AST node.

  Calls `ASTSourceSpan(node, doc_path, max_pos)` to get the full source
  extent, then clamps the start to `node.position()`. The clamp prevents
  positions from referenced types earlier in the file (e.g. in `type`
  aliases) from pulling the start before the declaration keyword.

  Returns `None` when `ASTSourceSpan` returns an inverted span.
  """
  fun tag apply(
    node: AST box,
    doc_path: String val,
    max_pos: (Position | None) = None)
    : (LspPositionRange | None)
  =>
    match \exhaustive\ ASTSourceSpan(node, doc_path, max_pos)
    | (let s: Position, let e: Position) =>
      let n_pos = node.position()
      let clamped_start = if s < n_pos then n_pos else s end
      LspPositionRange(
        LspPosition.from_ast_pos(clamped_start),
        LspPosition.from_ast_pos_end(e))
    | None =>
      None
    end
