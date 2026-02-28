use ast = "pony_compiler"

primitive ArrayLiteralFormat is ASTRule
  """
  Checks formatting of multiline array literal expressions per the style guide.

  For array literals that span multiple lines, checks:

  - The opening `[` must be the first non-whitespace on its line (no hanging
    indent after `=` or other expression context).
  - A space is required after `[` when there is content on the same line.

  Single-line array literals are not checked.
  """
  fun id(): String val => "style/array-literal-format"
  fun category(): String val => "style"

  fun description(): String val =>
    "multiline array literal formatting (bracket placement and spacing)"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [ast.TokenIds.tk_array()]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check formatting of a multiline array literal.
    """
    let open_line = node.line()
    let open_col = node.pos()

    // Determine if multiline by finding the max line in the subtree
    let mlv = _MaxLineVisitor(open_line)
    node.visit(mlv)
    if mlv.max_line == open_line then
      // Single-line array, nothing to check
      return recover val Array[Diagnostic val] end
    end

    let diags = recover iso Array[Diagnostic val] end

    try
      let line_text = source.lines(open_line - 1)?

      // Check 1: `[` must be first non-whitespace on its line.
      // If it's a hanging indent, report only that and skip spacing checks
      // since the user needs to restructure the line anyway.
      if not _is_first_nonws(line_text, open_col) then
        diags.push(Diagnostic(id(),
          "opening '[' of multiline array must be the first"
            + " non-whitespace on its line",
          source.rel_path, open_line, open_col))
        return consume diags
      end

      // Check 2: space after `[`
      let after_idx = open_col.usize() // 0-based index after `[`
      if after_idx < line_text.size() then
        if line_text(after_idx)? != ' ' then
          diags.push(Diagnostic(id(),
            "space required after '[' in multiline array",
            source.rel_path, open_line, open_col))
        end
      end
    end

    consume diags

  fun _is_first_nonws(line: String val, col: USize): Bool =>
    """
    Check if the character at `col` (1-based) is the first non-whitespace
    character on the line.
    """
    var i: USize = 0
    let target = col - 1
    while i < target do
      try
        let ch = line(i)?
        if (ch != ' ') and (ch != '\t') then
          return false
        end
      end
      i = i + 1
    end
    true
