use ast = "pony_compiler"

primitive OperatorSpacing is ASTRule
  """
  Checks whitespace around operators per the style guide.

  Binary operators require a space before and after. The `not` keyword requires
  a space after; before `not`, either a space or a non-alphanumeric character
  (e.g., `(`) is acceptable. Unary minus must NOT have a space after.
  """
  fun id(): String val => "style/operator-spacing"
  fun category(): String val => "style"

  fun description(): String val =>
    "binary operators need surrounding spaces; no space after unary '-'"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [
      // Arithmetic
      ast.TokenIds.tk_plus(); ast.TokenIds.tk_plus_tilde()
      ast.TokenIds.tk_minus(); ast.TokenIds.tk_minus_tilde()
      ast.TokenIds.tk_multiply(); ast.TokenIds.tk_multiply_tilde()
      ast.TokenIds.tk_divide(); ast.TokenIds.tk_divide_tilde()
      ast.TokenIds.tk_rem(); ast.TokenIds.tk_rem_tilde()
      ast.TokenIds.tk_mod(); ast.TokenIds.tk_mod_tilde()
      // Shifts
      ast.TokenIds.tk_lshift(); ast.TokenIds.tk_lshift_tilde()
      ast.TokenIds.tk_rshift(); ast.TokenIds.tk_rshift_tilde()
      // Comparisons
      ast.TokenIds.tk_eq(); ast.TokenIds.tk_eq_tilde()
      ast.TokenIds.tk_ne(); ast.TokenIds.tk_ne_tilde()
      ast.TokenIds.tk_lt(); ast.TokenIds.tk_lt_tilde()
      ast.TokenIds.tk_le(); ast.TokenIds.tk_le_tilde()
      ast.TokenIds.tk_gt(); ast.TokenIds.tk_gt_tilde()
      ast.TokenIds.tk_ge(); ast.TokenIds.tk_ge_tilde()
      // Logical
      ast.TokenIds.tk_and(); ast.TokenIds.tk_or(); ast.TokenIds.tk_xor()
      // Identity
      ast.TokenIds.tk_is(); ast.TokenIds.tk_isnt()
      // Assignment
      ast.TokenIds.tk_assign()
      // Unary
      ast.TokenIds.tk_unary_minus()
      ast.TokenIds.tk_unary_minus_tilde()
      // Keyword unary
      ast.TokenIds.tk_not()
    ]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Dispatch to the appropriate check based on whether the operator is
    unary minus, `not`, or a binary operator.
    """
    let token = node.id()
    if (token == ast.TokenIds.tk_unary_minus())
      or (token == ast.TokenIds.tk_unary_minus_tilde())
    then
      _check_unary_minus(node, source, token)
    elseif token == ast.TokenIds.tk_not() then
      _check_not(node, source)
    else
      _check_binary(node, source, token)
    end

  fun _check_binary(
    node: ast.AST box,
    source: SourceFile val,
    token: ast.TokenId)
    : Array[Diagnostic val] val
  =>
    """
    Binary operators require a space before and after. The "before" check is
    skipped when the operator is the first non-whitespace on its line
    (continuation line). The "after" check is skipped at end-of-line.
    """
    let op_line = node.line()
    let op_col = node.pos()
    let width = _op_width(token)
    try
      let line_text = source.lines(op_line - 1)?
      let starts_line = _is_first_nonws(line_text, op_col)
      let symbol = _op_symbol(token)

      // Check character before operator
      if (not starts_line) and (op_col > 1) then
        try
          if line_text((op_col - 2).usize())? != ' ' then
            return recover val
              [Diagnostic(id(),
                "space required before '" + symbol + "'",
                source.rel_path, op_line, op_col)]
            end
          end
        end
      end

      // Check character after operator
      let after_idx = ((op_col - 1) + width).usize()
      try
        if line_text(after_idx)? != ' ' then
          return recover val
            [Diagnostic(id(),
              "space required after '" + symbol + "'",
              source.rel_path, op_line, op_col)]
          end
        end
      end
    end
    recover val Array[Diagnostic val] end

  fun _check_unary_minus(
    node: ast.AST box,
    source: SourceFile val,
    token: ast.TokenId)
    : Array[Diagnostic val] val
  =>
    """
    Unary minus must NOT have a space after the operator (`-` or `-~`).
    """
    let op_line = node.line()
    let op_col = node.pos()
    let width: USize =
      if token == ast.TokenIds.tk_unary_minus_tilde() then 2 else 1 end
    let symbol: String val =
      if token == ast.TokenIds.tk_unary_minus_tilde() then "-~" else "-" end
    try
      let line_text = source.lines(op_line - 1)?
      let after_idx = ((op_col - 1) + width).usize()
      try
        if line_text(after_idx)? == ' ' then
          return recover val
            [Diagnostic(id(),
              "no space after unary '" + symbol + "'",
              source.rel_path, op_line, op_col)]
          end
        end
      end
    end
    recover val Array[Diagnostic val] end

  fun _check_not(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    `not` requires a space after. Before `not`, either a space or a
    non-alphanumeric character is acceptable (handles `(not x)` naturally).
    The "before" check is skipped when `not` is the first non-whitespace on
    its line.
    """
    let op_line = node.line()
    let op_col = node.pos()
    try
      let line_text = source.lines(op_line - 1)?
      let starts_line = _is_first_nonws(line_text, op_col)

      // Check character before 'not'
      if (not starts_line) and (op_col > 1) then
        try
          let before = line_text((op_col - 2).usize())?
          if (before != ' ') and _is_alphanum(before) then
            return recover val
              [Diagnostic(id(),
                "space required before 'not'",
                source.rel_path, op_line, op_col)]
            end
          end
        end
      end

      // Check character after 'not' (at op_col + 2, 0-based)
      let after_idx = (op_col + 2).usize()
      try
        if line_text(after_idx)? != ' ' then
          return recover val
            [Diagnostic(id(),
              "space required after 'not'",
              source.rel_path, op_line, op_col)]
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

  fun _is_alphanum(ch: U8): Bool =>
    """
    Check if a character is alphanumeric or underscore.
    """
    ((ch >= 'a') and (ch <= 'z'))
      or ((ch >= 'A') and (ch <= 'Z'))
      or ((ch >= '0') and (ch <= '9'))
      or (ch == '_')

  fun _op_width(token: ast.TokenId): USize =>
    """
    Return the character width of the operator token.
    """
    if (token == ast.TokenIds.tk_plus())
      or (token == ast.TokenIds.tk_minus())
      or (token == ast.TokenIds.tk_multiply())
      or (token == ast.TokenIds.tk_divide())
      or (token == ast.TokenIds.tk_rem())
      or (token == ast.TokenIds.tk_lt())
      or (token == ast.TokenIds.tk_gt())
      or (token == ast.TokenIds.tk_assign())
      or (token == ast.TokenIds.tk_unary_minus())
    then
      1
    elseif (token == ast.TokenIds.tk_plus_tilde())
      or (token == ast.TokenIds.tk_minus_tilde())
      or (token == ast.TokenIds.tk_multiply_tilde())
      or (token == ast.TokenIds.tk_divide_tilde())
      or (token == ast.TokenIds.tk_rem_tilde())
      or (token == ast.TokenIds.tk_mod())
      or (token == ast.TokenIds.tk_lshift())
      or (token == ast.TokenIds.tk_rshift())
      or (token == ast.TokenIds.tk_eq())
      or (token == ast.TokenIds.tk_ne())
      or (token == ast.TokenIds.tk_le())
      or (token == ast.TokenIds.tk_ge())
      or (token == ast.TokenIds.tk_or())
      or (token == ast.TokenIds.tk_is())
      or (token == ast.TokenIds.tk_lt_tilde())
      or (token == ast.TokenIds.tk_gt_tilde())
      or (token == ast.TokenIds.tk_unary_minus_tilde())
    then
      2
    elseif (token == ast.TokenIds.tk_mod_tilde())
      or (token == ast.TokenIds.tk_lshift_tilde())
      or (token == ast.TokenIds.tk_rshift_tilde())
      or (token == ast.TokenIds.tk_eq_tilde())
      or (token == ast.TokenIds.tk_ne_tilde())
      or (token == ast.TokenIds.tk_le_tilde())
      or (token == ast.TokenIds.tk_ge_tilde())
      or (token == ast.TokenIds.tk_not())
      or (token == ast.TokenIds.tk_and())
      or (token == ast.TokenIds.tk_xor())
    then
      3
    elseif token == ast.TokenIds.tk_isnt() then
      4
    else
      1 // fallback
    end

  fun _op_symbol(token: ast.TokenId): String val =>
    """
    Return the source-text representation of the operator for diagnostics.
    """
    if token == ast.TokenIds.tk_plus() then "+"
    elseif token == ast.TokenIds.tk_plus_tilde() then "+~"
    elseif token == ast.TokenIds.tk_minus() then "-"
    elseif token == ast.TokenIds.tk_minus_tilde() then "-~"
    elseif token == ast.TokenIds.tk_multiply() then "*"
    elseif token == ast.TokenIds.tk_multiply_tilde() then "*~"
    elseif token == ast.TokenIds.tk_divide() then "/"
    elseif token == ast.TokenIds.tk_divide_tilde() then "/~"
    elseif token == ast.TokenIds.tk_rem() then "%"
    elseif token == ast.TokenIds.tk_rem_tilde() then "%~"
    elseif token == ast.TokenIds.tk_mod() then "%%"
    elseif token == ast.TokenIds.tk_mod_tilde() then "%%~"
    elseif token == ast.TokenIds.tk_lshift() then "<<"
    elseif token == ast.TokenIds.tk_lshift_tilde() then "<<~"
    elseif token == ast.TokenIds.tk_rshift() then ">>"
    elseif token == ast.TokenIds.tk_rshift_tilde() then ">>~"
    elseif token == ast.TokenIds.tk_eq() then "=="
    elseif token == ast.TokenIds.tk_eq_tilde() then "==~"
    elseif token == ast.TokenIds.tk_ne() then "!="
    elseif token == ast.TokenIds.tk_ne_tilde() then "!=~"
    elseif token == ast.TokenIds.tk_lt() then "<"
    elseif token == ast.TokenIds.tk_lt_tilde() then "<~"
    elseif token == ast.TokenIds.tk_le() then "<="
    elseif token == ast.TokenIds.tk_le_tilde() then "<=~"
    elseif token == ast.TokenIds.tk_gt() then ">"
    elseif token == ast.TokenIds.tk_gt_tilde() then ">~"
    elseif token == ast.TokenIds.tk_ge() then ">="
    elseif token == ast.TokenIds.tk_ge_tilde() then ">=~"
    elseif token == ast.TokenIds.tk_and() then "and"
    elseif token == ast.TokenIds.tk_or() then "or"
    elseif token == ast.TokenIds.tk_xor() then "xor"
    elseif token == ast.TokenIds.tk_is() then "is"
    elseif token == ast.TokenIds.tk_isnt() then "isnt"
    elseif token == ast.TokenIds.tk_assign() then "="
    elseif token == ast.TokenIds.tk_not() then "not"
    elseif token == ast.TokenIds.tk_unary_minus() then "-"
    elseif token == ast.TokenIds.tk_unary_minus_tilde() then "-~"
    else "?"
    end
