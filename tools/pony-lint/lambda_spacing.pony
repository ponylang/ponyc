use ast = "pony_compiler"

primitive LambdaSpacing is ASTRule
  """
  Checks whitespace inside lambda braces per the style guide.

  Lambda expressions: no space after `{`; space before `}` only on single-line.
  Lambda types: no space after `{`, no space before `}`.
  Bare lambdas (`@{`) follow the same rules with the `{` one character later.
  """
  fun id(): String val => "style/lambda-spacing"
  fun category(): String val => "style"

  fun description(): String val =>
    "no space after '{' in lambdas; space before '}' only on single-line"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [
      ast.TokenIds.tk_lambda()
      ast.TokenIds.tk_barelambda()
      ast.TokenIds.tk_lambdatype()
      ast.TokenIds.tk_barelambdatype()
    ]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check spacing inside lambda/lambda-type braces.
    """
    let token = node.id()
    let is_bare =
      (token == ast.TokenIds.tk_barelambda())
        or (token == ast.TokenIds.tk_barelambdatype())
    let is_type =
      (token == ast.TokenIds.tk_lambdatype())
        or (token == ast.TokenIds.tk_barelambdatype())

    let op_line = node.line()
    let op_col = node.pos()
    // For bare lambdas, pos() points to `@`; the `{` is one character later
    let brace_col = if is_bare then op_col + 1 else op_col end

    try
      let line_text = source.lines(op_line - 1)?

      // Check character after `{`
      let after_brace_idx = brace_col.usize()
      try
        if line_text(after_brace_idx)? == ' ' then
          return recover val
            [ Diagnostic(
              id(),
              "no space after '{' in lambda",
              source.rel_path,
              op_line,
              brace_col)]
          end
        end
      end

      // Find closing `}` and check space before it
      match _find_closing_brace(source, op_line, brace_col)
      | (let close_line: USize, let close_col: USize) =>
        if is_type then
          // Lambda types: no space before `}`
          _check_no_space_before_brace(
            source,
            close_line,
            close_col,
            "no space before '}' in lambda type")
        else
          // Lambda expressions: space before `}` only on single-line
          let mlv = _MaxLineVisitor(op_line)
          node.visit(mlv)
          let single_line = mlv.max_line == op_line

          if single_line then
            // Single-line: must have space before `}`
            _check_space_before_brace(
              source,
              close_line,
              close_col)
          else
            // Multi-line: no space before `}`, unless `}` is first on its
            // line (indented on its own line is fine)
            try
              let close_text = source.lines(close_line - 1)?
              if not _is_first_nonws(close_text, close_col) then
                _check_no_space_before_brace(
                  source,
                  close_line,
                  close_col,
                  "no space before '}' in multi-line lambda")
              else
                recover val Array[Diagnostic val] end
              end
            else
              recover val Array[Diagnostic val] end
            end
          end
        end
      else
        recover val Array[Diagnostic val] end
      end
    else
      recover val Array[Diagnostic val] end
    end

  fun _check_space_before_brace(
    source: SourceFile val,
    close_line: USize,
    close_col: USize)
    : Array[Diagnostic val] val
  =>
    """
    Single-line lambda: char before `}` must be a space.
    """
    try
      let close_text = source.lines(close_line - 1)?
      if close_col > 1 then
        try
          if close_text((close_col - 2).usize())? != ' ' then
            return recover val
              [ Diagnostic(
                id(),
                "space required before '}' in single-line lambda",
                source.rel_path,
                close_line,
                close_col)]
            end
          end
        end
      end
    end
    recover val Array[Diagnostic val] end

  fun _check_no_space_before_brace(
    source: SourceFile val,
    close_line: USize,
    close_col: USize,
    msg: String val)
    : Array[Diagnostic val] val
  =>
    """
    Lambda type or multi-line lambda: char before `}` must NOT be a space.
    """
    try
      let close_text = source.lines(close_line - 1)?
      if close_col > 1 then
        try
          if close_text((close_col - 2).usize())? == ' ' then
            return recover val
              [ Diagnostic(
                id(),
                msg,
                source.rel_path,
                close_line,
                close_col)]
            end
          end
        end
      end
    end
    recover val Array[Diagnostic val] end

  fun _find_closing_brace(
    source: SourceFile val,
    open_line: USize,
    open_col: USize)
    : ((USize, USize) | None)
  =>
    """
    Scan source text from after the opening `{` to find the matching `}`,
    tracking brace depth. Returns `(line, col)` of the closing brace (both
    1-based), or `None` if not found.

    Note: this does not account for `{`/`}` inside string literals, but that
    is extremely rare in lambda expressions.
    """
    var depth: USize = 1
    var line_idx = open_line - 1
    var col_idx = open_col.usize() // 0-based, start after the `{`
    try
      while line_idx < source.lines.size() do
        let line_text = source.lines(line_idx)?
        while col_idx < line_text.size() do
          let ch = line_text(col_idx)?
          if ch == '{' then
            depth = depth + 1
          elseif ch == '}' then
            depth = depth - 1
            if depth == 0 then
              return (line_idx + 1, col_idx + 1)
            end
          end
          col_idx = col_idx + 1
        end
        line_idx = line_idx + 1
        col_idx = 0
      end
    end
    None

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
