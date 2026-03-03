use ast = "pony_compiler"

primitive AssignmentIndent is ASTRule
  """
  Flags assignment RHS with incorrect line placement or insufficient
  indentation.

  Two checks are performed:

  1. When an assignment's right-hand side spans multiple lines, the Pony style
     guide requires the RHS to start on the line after the `=`, not on the same
     line.
  2. When the RHS starts on the line after the `=`, it must be indented beyond
     the assignment line. Code at the same column or further left is a
     violation.

  Single-line assignments where the RHS is on the `=` line are always clean.
  """
  fun id(): String val => "style/assignment-indent"
  fun category(): String val => "style"

  fun description(): String val =>
    "assignment RHS must start on the line after '=' and be indented"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [ast.TokenIds.tk_assign()]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check whether an assignment RHS has correct placement and indentation.

    Gets the RHS (child 1) and compares its start line to the `=` line.

    If the RHS starts on a different line, verifies that it is indented beyond
    the assignment line — the RHS column must exceed the first non-whitespace
    column of the assignment line.

    If they are on the same line, uses `_MaxLineVisitor` to determine whether
    the RHS is multiline. A multiline RHS on the `=` line is a violation.
    """
    let assign_line = node.line()

    let rhs =
      try
        node(1)?
      else
        return recover val Array[Diagnostic val] end
      end

    if rhs.line() != assign_line then
      // RHS starts on a different line — verify sufficient indentation
      let assign_indent =
        try
          _count_leading_spaces(source.lines(assign_line - 1)?)
        else
          return recover val Array[Diagnostic val] end
        end
      if rhs.pos() <= (assign_indent + 1) then
        return recover val
          [ Diagnostic(
            id(),
            "assignment RHS must be indented relative to the assignment",
            source.rel_path,
            rhs.line(),
            rhs.pos())]
        end
      end
      return recover val Array[Diagnostic val] end
    end

    // RHS starts on the same line — check if it spans multiple lines
    let mlv = _MaxLineVisitor(assign_line)
    rhs.visit(mlv)

    if mlv.max_line > assign_line then
      // Multiline RHS starting on the `=` line — violation
      recover val
        [ Diagnostic(
          id(),
          "multiline assignment RHS should start on the line after '='",
          source.rel_path,
          assign_line,
          node.pos())]
      end
    else
      // Single-line RHS — clean
      recover val Array[Diagnostic val] end
    end

  fun _count_leading_spaces(line: String val): USize =>
    """
    Count the number of leading space characters on a line.
    """
    var count: USize = 0
    var i: USize = 0
    while i < line.size() do
      try
        if line(i)? == ' ' then
          count = count + 1
        else
          return count
        end
      else
        return count
      end
      i = i + 1
    end
    count
