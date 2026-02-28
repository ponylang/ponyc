use ast = "pony_compiler"

primitive DotSpacing is ASTRule
  """
  Checks spacing around `.` and `.>` operators.

  For `.` (TK_DOT): no spaces around the dot. When `.` starts a continuation
  line (first non-whitespace on the line), the "before" check is skipped.

  For `.>` (TK_CHAIN): spaces required around the operator, like other infix
  operators. When `.>` starts a continuation line, the "before" check is
  skipped.
  """
  fun id(): String val => "style/dot-spacing"
  fun category(): String val => "style"

  fun description(): String val =>
    "no spaces around '.'; '.>' spaced as infix operator"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [ast.TokenIds.tk_dot(); ast.TokenIds.tk_chain()]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check spacing around `.` or `.>` based on the node type.
    """
    if node.id() == ast.TokenIds.tk_dot() then
      _check_dot(node, source)
    else
      _check_chain(node, source)
    end

  fun _check_dot(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    `.` should have no spaces around it, unless it starts a continuation line.
    """
    let dot_line = node.line()
    let dot_col = node.pos()
    try
      let line_text = source.lines(dot_line - 1)?
      let starts_line = _is_first_nonws(line_text, dot_col)

      // Check character before .
      if (not starts_line) and (dot_col > 1) then
        try
          if line_text((dot_col - 2).usize())? == ' ' then
            return recover val
              [ Diagnostic(id(),
                "no space before '.'",
                source.rel_path, dot_line, dot_col)]
            end
          end
        end
      end

      // Check character after .
      try
        let after = line_text(dot_col.usize())?
        if after == ' ' then
          return recover val
            [ Diagnostic(id(),
              "no space after '.'",
              source.rel_path, dot_line, dot_col)]
          end
        end
      end
    end
    recover val Array[Diagnostic val] end

  fun _check_chain(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    `.>` should have spaces around it (infix operator), unless it starts a
    continuation line.
    """
    let chain_line = node.line()
    let chain_col = node.pos()
    try
      let line_text = source.lines(chain_line - 1)?
      let starts_line = _is_first_nonws(line_text, chain_col)

      // Check character before .>
      if (not starts_line) and (chain_col > 1) then
        try
          if line_text((chain_col - 2).usize())? != ' ' then
            return recover val
              [ Diagnostic(id(),
                "space required before '.>'",
                source.rel_path, chain_line, chain_col)]
            end
          end
        end
      end

      // Check character after .> (at col+2)
      let after_idx = (chain_col + 1).usize()
      try
        let after = line_text(after_idx)?
        if after != ' ' then
          return recover val
            [ Diagnostic(id(),
              "space required after '.>'",
              source.rel_path, chain_line, chain_col)]
          end
        end
      end
    end
    recover val Array[Diagnostic val] end

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
