use ast = "pony_compiler"

primitive TypeAliasFormat is ASTRule
  """
  Checks formatting of multiline type alias declarations per the style guide.

  For type aliases with a multiline union or intersection type, checks:

  - The opening `(` must be the first non-whitespace on its line (no hanging
    indent after `is`).
  - A space is required after `(`.
  - For each `|` or `&` that is the first non-whitespace on its line, a space
    is required after it.
  - A space is required before the closing `)`.

  Single-line type aliases and simple aliases (no union/intersection) are not
  checked.

  Note: only `|`/`&` at line starts (first non-whitespace) are checked.
  Mid-line separators (e.g., in nested types like `Map[String, (A | B)]`) are
  not checked.
  """
  fun id(): String val => "style/type-alias-format"
  fun category(): String val => "style"

  fun description(): String val =>
    "multiline type alias formatting (paren placement and spacing)"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [ast.TokenIds.tk_type()]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check formatting of a multiline type alias declaration.
    """
    // Get provides clause (child 3). If absent, nothing to check.
    let provides =
      try node(3)?
      else return recover val Array[Diagnostic val] end
      end
    if provides.id() == ast.TokenIds.tk_none() then
      return recover val Array[Diagnostic val] end
    end

    // Get the type expression (first child of TK_PROVIDES)
    let type_expr =
      try provides(0)?
      else return recover val Array[Diagnostic val] end
      end

    // Check if the type expression contains a union or intersection type
    if not _has_union_or_isect(type_expr) then
      return recover val Array[Diagnostic val] end
    end

    // Find the leftmost leaf type component
    let leaf = _leftmost_leaf(type_expr)

    // Scan backward from the leaf's position to find the opening `(`
    let leaf_line = leaf.line()
    let leaf_col = leaf.pos()

    (let open_line, let open_col) =
      match _find_open_paren(source, leaf_line, leaf_col)
      | (let ol: USize, let oc: USize) => (ol, oc)
      else
        return recover val Array[Diagnostic val] end
      end

    // Find the matching closing `)`
    (let close_line, let close_col) =
      match _find_closing_paren(source, open_line, open_col)
      | (let cl: USize, let cc: USize) => (cl, cc)
      else
        return recover val Array[Diagnostic val] end
      end

    // If single-line, nothing to check
    if open_line == close_line then
      return recover val Array[Diagnostic val] end
    end

    // Collect diagnostics for multiline type alias
    let diags = recover iso Array[Diagnostic val] end

    // Check 1: `(` must be first non-whitespace on its line.
    // If it's a hanging indent, report only that and skip spacing checks
    // since the user needs to restructure the line anyway.
    try
      let open_text = source.lines(open_line - 1)?
      if not _is_first_nonws(open_text, open_col) then
        diags.push(Diagnostic(
          id(),
          "opening '(' of multiline type must not be on the 'type ... is' line",
          source.rel_path,
          open_line,
          open_col))
        return consume diags
      end
    end

    // Check 2: space after `(`
    try
      let open_text = source.lines(open_line - 1)?
      let after_idx = open_col.usize() // 0-based index after `(`
      if after_idx < open_text.size() then
        if open_text(after_idx)? != ' ' then
          diags.push(Diagnostic(
            id(),
            "space required after '(' in multiline type",
            source.rel_path,
            open_line,
            open_col))
        end
      end
    end

    // Check 3: space after `|`/`&` at line starts between `(` and `)`
    var line_idx = open_line // start from line after `(`
    while line_idx < close_line do
      try
        let line_text = source.lines(line_idx)? // 0-based, so line_idx is
                                                // the line after open_line
        (let first_ch, let first_col) = _first_nonws_char(line_text)
        if (first_ch == '|') or (first_ch == '&') then
          let sep_str =
            recover val
              String .> push(first_ch)
            end
          // Check that next char is a space
          let after_sep = first_col + 1
          if after_sep < line_text.size() then
            if line_text(after_sep)? != ' ' then
              diags.push(Diagnostic(
                id(),
                "space required after '" + sep_str + "' in multiline type",
                source.rel_path,
                line_idx + 1,
                (first_col + 1).usize()))
            end
          end
        end
      end
      line_idx = line_idx + 1
    end

    // Check 4: space before `)`
    try
      let close_text = source.lines(close_line - 1)?
      if close_col > 1 then
        let before_idx = (close_col - 2).usize()
        if close_text(before_idx)? != ' ' then
          diags.push(Diagnostic(
            id(),
            "space required before ')' in multiline type",
            source.rel_path,
            close_line,
            close_col))
        end
      end
    end

    consume diags

  fun _has_union_or_isect(node: ast.AST box): Bool =>
    """
    Recursively check if the node is or contains a TK_UNIONTYPE or
    TK_ISECTTYPE.
    """
    let token = node.id()
    if (token == ast.TokenIds.tk_uniontype())
      or (token == ast.TokenIds.tk_isecttype())
    then
      return true
    end
    try
      _has_union_or_isect(node(0)?)
    else
      false
    end

  fun _leftmost_leaf(node: ast.AST box): ast.AST box =>
    """
    Walk child 0 through union/intersection nodes to find the leftmost
    leaf type component.
    """
    let token = node.id()
    if (token == ast.TokenIds.tk_uniontype())
      or (token == ast.TokenIds.tk_isecttype())
    then
      try
        return _leftmost_leaf(node(0)?)
      end
    end
    node

  fun _find_open_paren(
    source: SourceFile val,
    leaf_line: USize,
    leaf_col: USize)
    : ((USize, USize) | None)
  =>
    """
    Scan backward from the leftmost leaf's position to find the `(` that
    opens the type expression. Checks the leaf's line first, then the
    previous line.
    """
    try
      let line_text = source.lines(leaf_line - 1)?
      var col: USize = (leaf_col - 2).usize() // 0-based, before the leaf
      while true do
        let ch = line_text(col)?
        if ch == '(' then
          return (leaf_line, col + 1) // 1-based column
        end
        if col == 0 then break end
        col = col - 1
      end
    end
    // Check previous line
    if leaf_line > 1 then
      try
        let prev_text = source.lines(leaf_line - 2)?
        var col: USize = prev_text.size()
        if col > 0 then
          col = col - 1
          while true do
            let ch = prev_text(col)?
            if ch == '(' then
              return (leaf_line - 1, col + 1) // 1-based
            end
            if col == 0 then break end
            col = col - 1
          end
        end
      end
    end
    None

  fun _find_closing_paren(
    source: SourceFile val,
    open_line: USize,
    open_col: USize)
    : ((USize, USize) | None)
  =>
    """
    Scan source text from after the opening `(` to find the matching `)`,
    tracking paren depth. Returns `(line, col)` both 1-based, or `None`.
    """
    var depth: USize = 1
    var line_idx = open_line - 1
    var col_idx = open_col.usize() // 0-based, start after the `(`
    try
      while line_idx < source.lines.size() do
        let line_text = source.lines(line_idx)?
        while col_idx < line_text.size() do
          let ch = line_text(col_idx)?
          if ch == '(' then
            depth = depth + 1
          elseif ch == ')' then
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

  fun _first_nonws_char(line: String val): (U8, USize) =>
    """
    Find the first non-whitespace character on a line.
    Returns `(char, 0-based-index)` or `(0, 0)` if blank.
    """
    var i: USize = 0
    while i < line.size() do
      try
        let ch = line(i)?
        if (ch != ' ') and (ch != '\t') then
          return (ch, i)
        end
      end
      i = i + 1
    end
    (0, 0)
