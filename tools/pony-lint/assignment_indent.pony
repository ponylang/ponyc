use ast = "pony_compiler"

primitive AssignmentIndent is ASTRule
  """
  Flags multiline assignment RHS that starts on the same line as the `=`.

  When an assignment's right-hand side spans multiple lines, the Pony style
  guide requires the RHS to start on the line after the `=`, not on the same
  line. Single-line assignments are always clean.
  """
  fun id(): String val => "style/assignment-indent"
  fun category(): String val => "style"

  fun description(): String val =>
    "multiline assignment RHS must start on the line after '='"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [ast.TokenIds.tk_assign()]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check whether a multiline RHS begins on the same line as the `=`.

    Gets the RHS (child 1) and compares its start line to the `=` line. If
    they differ, the RHS already starts on the next line — clean. If they
    match, uses `_MaxLineVisitor` to determine whether the RHS is multiline.
    A multiline RHS on the `=` line is a violation.
    """
    let assign_line = node.line()

    let rhs =
      try
        node(1)?
      else
        return recover val Array[Diagnostic val] end
      end

    // RHS starts on a different line than `=` — already correct
    if rhs.line() != assign_line then
      return recover val Array[Diagnostic val] end
    end

    // RHS starts on the same line — check if it spans multiple lines
    let mlv = _MaxLineVisitor(assign_line)
    rhs.visit(mlv)

    if mlv.max_line > assign_line then
      // Multiline RHS starting on the `=` line — violation
      recover val
        [Diagnostic(id(),
          "multiline assignment RHS should start on the line after '='",
          source.rel_path, assign_line, node.pos())]
      end
    else
      // Single-line RHS — clean
      recover val Array[Diagnostic val] end
    end
