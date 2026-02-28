use ast = "pony_compiler"

primitive CallArgumentFormat is ASTRule
  """
  Checks formatting of multiline function call arguments per the style guide.

  For calls with two or more positional arguments that span multiple lines,
  checks:

  - No arguments start on the `(` line — arguments should begin on the
    next line.
  - Arguments are either all on one continuation line, or each on its own
    line.

  Single-line calls, single-argument calls, and named-argument-only calls
  are not checked. The `where` clause is allowed on its own line and is not
  subject to layout checks.

  This rule does not check whether all-on-one-line arguments fit within the
  80-column limit — `style/line-length` catches that separately.

  These rules apply to both regular calls and FFI calls.
  """
  fun id(): String val => "style/call-argument-format"
  fun category(): String val => "style"

  fun description(): String val =>
    "multiline call argument formatting"
      + " (argument layout and placement)"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [ ast.TokenIds.tk_call()
      ast.TokenIds.tk_fficall() ]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check formatting of multiline call arguments.
    """
    let is_fficall = node.id() == ast.TokenIds.tk_fficall()

    // Get TK_POSITIONALARGS: child 1 for TK_CALL, child 2 for TK_FFICALL
    let pos_args_idx: USize = if is_fficall then 2 else 1 end
    let pos_args =
      try
        let child = node(pos_args_idx)?
        if child.id() == ast.TokenIds.tk_none() then
          return recover val Array[Diagnostic val] end
        end
        if child.id() != ast.TokenIds.tk_positionalargs() then
          return recover val Array[Diagnostic val] end
        end
        child
      else
        return recover val Array[Diagnostic val] end
      end

    let num_args = pos_args.num_children()
    if num_args < 2 then
      return recover val Array[Diagnostic val] end
    end

    // Find the '(' line and column
    (let open_line, let open_col) =
      _find_open_paren(node, source)

    // Determine if multiline using _MaxLineVisitor seeded with '(' line
    let mlv = _MaxLineVisitor(open_line)
    node.visit(mlv)
    if mlv.max_line == open_line then
      // Single-line call
      return recover val Array[Diagnostic val] end
    end

    let diags = recover iso Array[Diagnostic val] end

    // Collect start lines for each positional arg
    let arg_lines = Array[USize](num_args)
    var i: USize = 0
    while i < num_args do
      try
        arg_lines.push(_arg_start_line(pos_args(i)?))
      else _Unreachable()
      end
      i = i + 1
    end

    // Check 1: no arg should start on the '(' line
    for arg_line in arg_lines.values() do
      if arg_line == open_line then
        diags.push(Diagnostic(
          id(),
          "in a multiline call, arguments should start"
            + " on the line after '('",
          source.rel_path,
          open_line,
          open_col))
        break // one diagnostic for this check
      end
    end

    // Check 2: all on one line or each on own line
    let distinct = _count_distinct(arg_lines)
    if (distinct != 1) and (distinct != arg_lines.size()) then
      diags.push(Diagnostic(
        id(),
        "multiline call arguments must all be on one line"
          + " or each on its own line",
        source.rel_path,
        open_line,
        open_col))
    end

    consume diags

  fun _find_open_paren(
    node: ast.AST box,
    source: SourceFile val)
    : (USize, USize)
  =>
    """
    Return the `(1-based line, 1-based col)` of the `(` for a call.

    For TK_CALL the node position is at `(` (INFIX_BUILD). For TK_FFICALL
    the node position is at `@`, so we scan forward through source text to
    find the first `(` character.
    """
    if node.id() == ast.TokenIds.tk_call() then
      return (node.line(), node.pos())
    end

    // TK_FFICALL: scan forward from '@' to find '('
    let start_line_idx = node.line() - 1 // 0-based
    var line_idx = start_line_idx
    while line_idx < source.lines.size() do
      try
        let line_text = source.lines(line_idx)?
        let col_start: USize =
          if line_idx == start_line_idx then
            node.pos() // 1-based col of '@', so 0-based index after '@'
          else
            0
          end
        var j: USize = col_start
        while j < line_text.size() do
          if line_text(j)? == '(' then
            return (line_idx + 1, j + 1) // convert to 1-based
          end
          j = j + 1
        end
      end
      line_idx = line_idx + 1
    end
    (node.line(), node.pos()) // fallback

  fun _arg_start_line(node: ast.AST box): USize =>
    """
    Find the visual start line of an argument expression.

    Arguments are wrapped in TK_SEQ nodes, and the actual expression may be
    an infix-built node (like TK_CALL or TK_DOT) whose position is at the
    operator rather than the visual start. This function unwraps to find
    the leftmost source position.
    """
    if node.id() == ast.TokenIds.tk_seq() then
      try return _arg_start_line(node(0)?) end
    end
    if node.infix_node() then
      try return _arg_start_line(node(0)?) end
    end
    node.line()

  fun _count_distinct(lines: Array[USize]): USize =>
    """
    Count distinct values in an array of line numbers.
    """
    var count: USize = 0
    var i: USize = 0
    while i < lines.size() do
      try
        let line = lines(i)?
        var seen = false
        var j: USize = 0
        while j < i do
          try
            if lines(j)? == line then
              seen = true
              break
            end
          end
          j = j + 1
        end
        if not seen then count = count + 1 end
      end
      i = i + 1
    end
    count
