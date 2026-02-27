use ast = "pony_compiler"

primitive BlankLines is ASTRule
  """
  Checks blank-line conventions within and between entities.

  Within entities:
  - No blank line between the entity declaration and the first body content
    (docstring or first member).
  - No blank lines between consecutive fields.
  - Exactly one blank line before each method, with an exception: when both
    the preceding member and the method are one-liners, zero blank lines are
    allowed.

  Between entities:
  - Exactly one blank line between consecutive entities, with the same
    one-liner exception.

  Between-entity checks use `check_module()`, called once per module after
  AST traversal.
  """
  fun id(): String val => "style/blank-lines"
  fun category(): String val => "style"

  fun description(): String val =>
    "blank line conventions within and between entities"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [
      ast.TokenIds.tk_class(); ast.TokenIds.tk_actor()
      ast.TokenIds.tk_primitive(); ast.TokenIds.tk_struct()
      ast.TokenIds.tk_trait(); ast.TokenIds.tk_interface()
    ]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check blank-line conventions within an entity: no blank line before
    first body content, no blank lines between fields, one blank line before
    methods (with one-liner exception).
    """
    let result = recover iso Array[Diagnostic val] end

    // Find first body content line — docstring (child 6) or first member
    let docstring_line = _docstring_line(node)
    let members_node = try node(4)? else
      return recover val Array[Diagnostic val] end
    end
    if members_node.id() != ast.TokenIds.tk_members() then
      return recover val Array[Diagnostic val] end
    end

    // Determine first body content line
    let first_content_line: USize =
      match docstring_line
      | let dl: USize => dl
      else
        // No docstring — use first member
        if members_node.num_children() == 0 then
          return recover val Array[Diagnostic val] end
        end
        try members_node(0)?.line() else
          return recover val Array[Diagnostic val] end
        end
      end

    // Check no blank line before first body content
    if first_content_line > 1 then
      try
        let prev_line = source.lines(first_content_line - 2)?
        if _is_blank(prev_line) then
          result.push(Diagnostic(id(),
            "no blank line before first body content",
            source.rel_path, first_content_line - 1, 1))
        end
      end
    end

    // Check blank lines between members
    let num_members = members_node.num_children()
    if num_members < 2 then
      return consume result
    end

    var prev_idx: USize = 0
    while prev_idx < (num_members - 1) do
      try
        let prev_member = members_node(prev_idx)?
        let curr_member = members_node(prev_idx + 1)?
        let prev_end = _max_line(prev_member)
        let curr_start = curr_member.line()
        let blanks = _count_blank_lines(source, prev_end, curr_start)
        let prev_is_field = _is_field(prev_member.id())
        let curr_is_field = _is_field(curr_member.id())
        let curr_is_method = _is_method(curr_member.id())

        if prev_is_field and curr_is_field then
          // No blank lines between consecutive fields
          if blanks > 0 then
            result.push(Diagnostic(id(),
              "no blank lines between fields",
              source.rel_path, curr_start, curr_member.pos()))
          end
        elseif curr_is_method then
          // One blank line before methods
          let prev_is_method = _is_method(prev_member.id())
          let prev_one_liner = _is_one_liner(prev_member)
          let curr_one_liner = _is_one_liner(curr_member)
          if prev_is_method and prev_one_liner and curr_one_liner then
            // One-liner exception: both methods are one-liners
            if blanks > 1 then
              result.push(Diagnostic(id(),
                "at most 1 blank line before method",
                source.rel_path, curr_start, curr_member.pos()))
            end
          else
            if blanks != 1 then
              result.push(Diagnostic(id(),
                "exactly 1 blank line before method",
                source.rel_path, curr_start, curr_member.pos()))
            end
          end
        end
      end
      prev_idx = prev_idx + 1
    end

    consume result

  fun check_module(
    entities: Array[(String val, ast.TokenId, USize, USize)] val,
    source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check that consecutive entities are separated by exactly one blank line,
    with the one-liner exception.
    """
    if entities.size() < 2 then
      return recover val Array[Diagnostic val] end
    end

    // Sort by start_line (entities should already be in order from the
    // visitor, but be defensive)
    let sorted = Array[(String val, ast.TokenId, USize, USize)]
    for e in entities.values() do sorted.push(e) end
    _sort_by_start_line(sorted)

    let result = recover iso Array[Diagnostic val] end
    var i: USize = 0
    while i < (sorted.size() - 1) do
      try
        (_, _, let a_start, let a_end) = sorted(i)?
        (_, _, let b_start, let b_end) = sorted(i + 1)?
        let blanks = _count_blank_lines(source, a_end, b_start)
        let a_one_liner = a_end == a_start
        let b_one_liner = b_end == b_start
        if a_one_liner and b_one_liner then
          // Both one-liners — 0 or 1 blank lines allowed
          if blanks > 1 then
            result.push(Diagnostic(id(),
              "at most 1 blank line between entities",
              source.rel_path, b_start, 1))
          end
        else
          if blanks != 1 then
            result.push(Diagnostic(id(),
              "exactly 1 blank line between entities",
              source.rel_path, b_start, 1))
          end
        end
      end
      i = i + 1
    end
    consume result

  fun _docstring_line(node: ast.AST box): (USize | None) =>
    """
    Get the line of the entity docstring (child 6), or None if absent.
    """
    try
      let doc = node(6)?
      if doc.id() != ast.TokenIds.tk_none() then
        return doc.line()
      end
    end
    None

  fun _is_field(token_id: ast.TokenId): Bool =>
    (token_id == ast.TokenIds.tk_flet())
      or (token_id == ast.TokenIds.tk_fvar())
      or (token_id == ast.TokenIds.tk_embed())

  fun _is_method(token_id: ast.TokenId): Bool =>
    (token_id == ast.TokenIds.tk_fun())
      or (token_id == ast.TokenIds.tk_new())
      or (token_id == ast.TokenIds.tk_be())

  fun _max_line(node: ast.AST box): USize =>
    """
    Find the maximum line number reached by a node and its descendants.
    """
    let mlv = _MaxLineVisitor(node.line())
    node.visit(mlv)
    mlv.max_line

  fun _is_one_liner(node: ast.AST box): Bool =>
    """
    A member is a one-liner if all descendants are on the same line.
    """
    _max_line(node) == node.line()

  fun _count_blank_lines(
    source: SourceFile val,
    after_line: USize,
    before_line: USize)
    : USize
  =>
    """
    Count blank lines between `after_line` and `before_line` (1-based,
    exclusive on both ends).
    """
    if before_line <= (after_line + 1) then return 0 end
    var count: USize = 0
    var line_idx = after_line // 0-based index of next line to check
    while line_idx < (before_line - 1) do
      try
        if _is_blank(source.lines(line_idx)?) then
          count = count + 1
        end
      end
      line_idx = line_idx + 1
    end
    count

  fun _is_blank(line: String val): Bool =>
    """
    Check if a line is blank (empty or all whitespace).
    """
    var i: USize = 0
    while i < line.size() do
      try
        let ch = line(i)?
        if (ch != ' ') and (ch != '\t') then return false end
      end
      i = i + 1
    end
    true

  fun _sort_by_start_line(
    arr: Array[(String val, ast.TokenId, USize, USize)])
  =>
    """
    Simple insertion sort by start_line (element _3).
    """
    var i: USize = 1
    while i < arr.size() do
      try
        let key = arr(i)?
        var j = i
        while j > 0 do
          let prev = arr(j - 1)?
          if prev._3 <= key._3 then break end
          arr(j)? = prev
          j = j - 1
        end
        arr(j)? = key
      end
      i = i + 1
    end
