use ast = "pony_compiler"

primitive ControlStructureAlignment is ASTRule
  """
  Flags multi-line control structures whose keywords are not vertically
  aligned.

  The Pony style guide requires control structure keywords to start at the
  same column:

  * `if` / `elseif` / `else` / `end`
  * `ifdef` / `elseif` / `else` / `end`
  * `while` / `else` / `end`
  * `for` / `else` / `end`
  * `try` / `else` / `then` / `end`
  * `repeat` / `until` / `else` / `end`
  * `with` / `end`

  Single-line structures are exempt. The `do` keyword is not checked because
  it always appears on the same line as its opening keyword.
  """
  fun id(): String val => "style/control-structure-alignment"
  fun category(): String val => "style"

  fun description(): String val =>
    "control structure keywords must be vertically aligned"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [ ast.TokenIds.tk_if()
      ast.TokenIds.tk_while()
      ast.TokenIds.tk_for()
      ast.TokenIds.tk_repeat()
      ast.TokenIds.tk_try()
      ast.TokenIds.tk_with()
      ast.TokenIds.tk_ifdef() ]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Dispatch to structure-specific checking based on the node's token type.
    Skips single-line structures and elseif TK_IF nodes (handled by their
    parent if chain).
    """
    let token_id = node.id()

    // Skip elseif TK_IF nodes — the parent if handles the full chain.
    // The parser represents `elseif` as a nested TK_IF. Distinguish by
    // checking whether the source at the node's position starts with 'e'.
    if token_id == ast.TokenIds.tk_if() then
      try
        let line_text = source.lines(node.line() - 1)?
        let col_idx = node.pos() - 1
        if col_idx < line_text.size() then
          if line_text(col_idx)? == 'e' then
            return recover val Array[Diagnostic val] end
          end
        end
      end
    end

    // Similarly skip elseif TK_IFDEF nodes
    if token_id == ast.TokenIds.tk_ifdef() then
      try
        let line_text = source.lines(node.line() - 1)?
        let col_idx = node.pos() - 1
        if col_idx < line_text.size() then
          if line_text(col_idx)? == 'e' then
            return recover val Array[Diagnostic val] end
          end
        end
      end
    end

    // Skip single-line structures
    let mlv = _MaxLineVisitor(node.line())
    node.visit(mlv)
    if mlv.max_line == node.line() then
      return recover val Array[Diagnostic val] end
    end

    let expected_col = node.pos()
    let opening = _opening_keyword_name(token_id)

    if token_id == ast.TokenIds.tk_if() then
      _check_if(node, source, expected_col, opening)
    elseif token_id == ast.TokenIds.tk_ifdef() then
      _check_ifdef(node, source, expected_col, opening)
    elseif token_id == ast.TokenIds.tk_while() then
      _check_while(node, source, expected_col, opening)
    elseif token_id == ast.TokenIds.tk_for() then
      _check_for(node, source, expected_col, opening)
    elseif token_id == ast.TokenIds.tk_repeat() then
      _check_repeat(node, source, expected_col, opening)
    elseif token_id == ast.TokenIds.tk_try() then
      _check_try(node, source, expected_col, opening)
    elseif token_id == ast.TokenIds.tk_with() then
      _check_end(node, source, expected_col, opening)
    else
      recover val Array[Diagnostic val] end
    end

  fun _opening_keyword_name(token_id: ast.TokenId): String val =>
    """
    Return the source keyword name for a given token type.
    """
    if token_id == ast.TokenIds.tk_if() then "if"
    elseif token_id == ast.TokenIds.tk_ifdef() then "ifdef"
    elseif token_id == ast.TokenIds.tk_while() then "while"
    elseif token_id == ast.TokenIds.tk_for() then "for"
    elseif token_id == ast.TokenIds.tk_repeat() then "repeat"
    elseif token_id == ast.TokenIds.tk_try() then "try"
    elseif token_id == ast.TokenIds.tk_with() then "with"
    else "unknown"
    end

  fun _check_if(
    node: ast.AST box,
    source: SourceFile val,
    expected_col: USize,
    opening: String val)
    : Array[Diagnostic val] val
  =>
    """
    Check if/elseif/else/end alignment.

    TK_IF children: [0] condition, [1] then body, [2] else clause.
    The else clause is TK_IF for elseif, a seq for else, or TK_NONE.
    """
    let result = recover iso Array[Diagnostic val] end

    // Walk the if/elseif chain
    var current = node
    while true do
      try
        let else_clause = current(2)?
        if else_clause.id() == ast.TokenIds.tk_if() then
          let actual_col = else_clause.pos()
          if actual_col != expected_col then
            result.push(_misalignment_diag(
              "elseif",
              opening,
              expected_col,
              source.rel_path,
              else_clause.line(),
              actual_col))
          end
          current = else_clause
        elseif else_clause.id() != ast.TokenIds.tk_none() then
          let d =
            _find_keyword_above(
              source,
              else_clause.line(),
              "else",
              current.line(),
              expected_col,
              opening)
          match d
          | let diag: Diagnostic val => result.push(diag)
          end
          break
        else
          break
        end
      else
        break
      end
    end

    let d = _find_end(node, source, expected_col, opening)
    match d
    | let diag: Diagnostic val => result.push(diag)
    end

    consume result

  fun _check_ifdef(
    node: ast.AST box,
    source: SourceFile val,
    expected_col: USize,
    opening: String val)
    : Array[Diagnostic val] val
  =>
    """
    Check ifdef/elseif/else/end alignment.

    TK_IFDEF children: [0] condition, [1] then body, [2] else clause,
    [3] extra cond.
    The else clause is TK_IFDEF for elseif, a seq for else, or TK_NONE.
    """
    let result = recover iso Array[Diagnostic val] end

    var current = node
    while true do
      try
        let else_clause = current(2)?
        if else_clause.id() == ast.TokenIds.tk_ifdef() then
          let actual_col = else_clause.pos()
          if actual_col != expected_col then
            result.push(_misalignment_diag(
              "elseif",
              opening,
              expected_col,
              source.rel_path,
              else_clause.line(),
              actual_col))
          end
          current = else_clause
        elseif else_clause.id() != ast.TokenIds.tk_none() then
          let d =
            _find_keyword_above(
              source,
              else_clause.line(),
              "else",
              current.line(),
              expected_col,
              opening)
          match d
          | let diag: Diagnostic val => result.push(diag)
          end
          break
        else
          break
        end
      else
        break
      end
    end

    let d = _find_end(node, source, expected_col, opening)
    match d
    | let diag: Diagnostic val => result.push(diag)
    end

    consume result

  fun _check_while(
    node: ast.AST box,
    source: SourceFile val,
    expected_col: USize,
    opening: String val)
    : Array[Diagnostic val] val
  =>
    """
    Check while/else/end alignment.

    TK_WHILE children: [0] condition, [1] body, [2] else clause.
    """
    let result = recover iso Array[Diagnostic val] end

    try
      let else_clause = node(2)?
      if else_clause.id() != ast.TokenIds.tk_none() then
        let d =
          _find_keyword_above(
            source,
            else_clause.line(),
            "else",
            node.line(),
            expected_col,
            opening)
        match d
        | let diag: Diagnostic val => result.push(diag)
        end
      end
    end

    let d = _find_end(node, source, expected_col, opening)
    match d
    | let diag: Diagnostic val => result.push(diag)
    end

    consume result

  fun _check_for(
    node: ast.AST box,
    source: SourceFile val,
    expected_col: USize,
    opening: String val)
    : Array[Diagnostic val] val
  =>
    """
    Check for/else/end alignment.

    TK_FOR children: [0] idseq, [1] iterator, [2] body, [3] else clause.
    """
    let result = recover iso Array[Diagnostic val] end

    try
      let else_clause = node(3)?
      if else_clause.id() != ast.TokenIds.tk_none() then
        let d =
          _find_keyword_above(
            source,
            else_clause.line(),
            "else",
            node.line(),
            expected_col,
            opening)
        match d
        | let diag: Diagnostic val => result.push(diag)
        end
      end
    end

    let d = _find_end(node, source, expected_col, opening)
    match d
    | let diag: Diagnostic val => result.push(diag)
    end

    consume result

  fun _check_repeat(
    node: ast.AST box,
    source: SourceFile val,
    expected_col: USize,
    opening: String val)
    : Array[Diagnostic val] val
  =>
    """
    Check repeat/until/else/end alignment.

    TK_REPEAT children: [0] body, [1] until condition, [2] else clause.
    """
    let result = recover iso Array[Diagnostic val] end

    try
      let until_cond = node(1)?
      if until_cond.id() != ast.TokenIds.tk_none() then
        let d =
          _find_keyword_above(
            source,
            until_cond.line(),
            "until",
            node.line(),
            expected_col,
            opening)
        match d
        | let diag: Diagnostic val => result.push(diag)
        end
      end
    end

    try
      let else_clause = node(2)?
      if else_clause.id() != ast.TokenIds.tk_none() then
        try
          let until_cond = node(1)?
          let d =
            _find_keyword_above(
              source,
              else_clause.line(),
              "else",
              until_cond.line(),
              expected_col,
              opening)
          match d
          | let diag: Diagnostic val => result.push(diag)
          end
        end
      end
    end

    let d = _find_end(node, source, expected_col, opening)
    match d
    | let diag: Diagnostic val => result.push(diag)
    end

    consume result

  fun _check_try(
    node: ast.AST box,
    source: SourceFile val,
    expected_col: USize,
    opening: String val)
    : Array[Diagnostic val] val
  =>
    """
    Check try/else/then/end alignment.

    TK_TRY children: [0] body, [1] else clause, [2] then clause.
    """
    let result = recover iso Array[Diagnostic val] end

    try
      let else_clause = node(1)?
      if else_clause.id() != ast.TokenIds.tk_none() then
        let d =
          _find_keyword_above(
            source,
            else_clause.line(),
            "else",
            node.line(),
            expected_col,
            opening)
        match d
        | let diag: Diagnostic val => result.push(diag)
        end
      end
    end

    try
      let then_clause = node(2)?
      if then_clause.id() != ast.TokenIds.tk_none() then
        try
          let else_clause = node(1)?
          if else_clause.id() != ast.TokenIds.tk_none() then
            let d =
              _find_keyword_above(
                source,
                then_clause.line(),
                "then",
                else_clause.line(),
                expected_col,
                opening)
            match d
            | let diag: Diagnostic val => result.push(diag)
            end
          else
            let d =
              _find_keyword_above(
                source,
                then_clause.line(),
                "then",
                node.line(),
                expected_col,
                opening)
            match d
            | let diag: Diagnostic val => result.push(diag)
            end
          end
        end
      end
    end

    let d = _find_end(node, source, expected_col, opening)
    match d
    | let diag: Diagnostic val => result.push(diag)
    end

    consume result

  fun _misalignment_diag(
    keyword: String val,
    opening: String val,
    expected_col: USize,
    file: String val,
    line: USize,
    actual_col: USize)
    : Diagnostic val
  =>
    """
    Create a diagnostic for a misaligned keyword.
    """
    Diagnostic(
      id(),
      "'" + keyword + "' should align with '" + opening
        + "' keyword (column " + expected_col.string() + ")",
      file,
      line,
      actual_col)

  fun _find_keyword_above(
    source: SourceFile val,
    from_line: USize,
    keyword: String val,
    limit_line: USize,
    expected_col: USize,
    opening: String val)
    : (Diagnostic val | None)
  =>
    """
    Backward-scan from `from_line` looking for `keyword` as the first
    non-whitespace on a line, stopping at `limit_line`. Returns a diagnostic
    if found at the wrong column, or None if aligned or not found.
    """
    var line_num = from_line
    while line_num >= limit_line do
      try
        let line_text = source.lines(line_num - 1)?
        (let word, let col) = _first_nonws_word(line_text)
        if word == keyword then
          if col != expected_col then
            return _misalignment_diag(
              keyword,
              opening,
              expected_col,
              source.rel_path,
              line_num,
              col)
          end
          return None
        end
      end
      if line_num == 0 then break end
      line_num = line_num - 1
    end
    None

  fun _find_end(
    node: ast.AST box,
    source: SourceFile val,
    expected_col: USize,
    opening: String val)
    : (Diagnostic val | None)
  =>
    """
    Forward-scan from the opening keyword line to find the matching `end`.
    Tracks block depth using all keywords on each line (not just the first
    non-whitespace word) to correctly handle single-line blocks like
    `if cond then expr end`.
    """
    var depth: ISize = 1
    var line_num = node.line() + 1
    var in_docstring = false

    while line_num <= source.lines.size() do
      try
        let line_text = source.lines(line_num - 1)?
        // Track triple-quoted strings by checking raw line content.
        // _first_nonws_word stops at '"', so it can't detect '"""'.
        if _starts_with_triple_quote(line_text) then
          in_docstring = not in_docstring
          line_num = line_num + 1
          continue
        end

        (let word, let col) = _first_nonws_word(line_text)

        if in_docstring then
          line_num = line_num + 1
          continue
        end

        if word.size() > 0 then
          let delta = _line_depth_delta(line_text)
          depth = depth + delta

          if depth <= 0 then
            // The matching end is on this line
            if word == "end" then
              if col != expected_col then
                return _misalignment_diag(
                  "end",
                  opening,
                  expected_col,
                  source.rel_path,
                  line_num,
                  col)
              end
            end
            return None
          end
        end
      end
      line_num = line_num + 1
    end
    None

  fun _check_end(
    node: ast.AST box,
    source: SourceFile val,
    expected_col: USize,
    opening: String val)
    : Array[Diagnostic val] val
  =>
    """
    Check end alignment for structures that only need an end check (with).
    """
    let d = _find_end(node, source, expected_col, opening)
    match d
    | let diag: Diagnostic val =>
      recover val [diag] end
    else
      recover val Array[Diagnostic val] end
    end

  fun _first_nonws_word(line: String val): (String val, USize) =>
    """
    Extract the first non-whitespace word from a line, returning the word
    and its 1-based column position. Returns `("", 0)` for blank lines.
    """
    var i: USize = 0
    while i < line.size() do
      try
        let ch = line(i)?
        if (ch != ' ') and (ch != '\t') then break end
      end
      i = i + 1
    end

    if i >= line.size() then
      return ("", 0)
    end

    let col = i + 1 // 1-based
    let start = i

    while i < line.size() do
      try
        let ch = line(i)?
        if (ch == ' ') or (ch == '\t') or (ch == '(') or (ch == '\\')
          or (ch == '"')
        then
          break
        end
      end
      i = i + 1
    end

    (recover val line.substring(
      ISize.from[USize](start),
      ISize.from[USize](i)) end, col)

  fun _line_depth_delta(line: String val): ISize =>
    """
    Count block opener and `end` keywords on a line and return the net
    depth change. Extracts identifier words (alphanumeric + underscore),
    skipping content inside string literals and line comments.
    """
    var delta: ISize = 0
    var i: USize = 0
    var in_string = false

    while i < line.size() do
      try
        let ch = line(i)?

        // Toggle string state on double quotes
        if ch == '"' then
          in_string = not in_string
          i = i + 1
          continue
        end

        // Skip string contents, handling backslash escapes
        if in_string then
          if ch == '\\' then i = i + 1 end
          i = i + 1
          continue
        end

        // Skip line comments
        if (ch == '/') and ((i + 1) < line.size()) then
          if line(i + 1)? == '/' then
            break
          end
        end

        // Extract identifier words
        if _is_ident_char(ch) then
          let word_start = i
          while i < line.size() do
            if not _is_ident_char(line(i)?) then break end
            i = i + 1
          end
          let word =
            recover val
              line.substring(
                ISize.from[USize](word_start),
                ISize.from[USize](i))
            end
          if _is_block_opener(word) then delta = delta + 1 end
          if word == "end" then delta = delta - 1 end
        else
          i = i + 1
        end
      else
        i = i + 1
      end
    end
    delta

  fun _starts_with_triple_quote(line: String val): Bool =>
    """
    Check if the first non-whitespace content on a line is `\"\"\"`.
    """
    var i: USize = 0
    while i < line.size() do
      try
        let ch = line(i)?
        if (ch != ' ') and (ch != '\t') then
          return (((i + 2) < line.size())
            and (ch == '"')
            and (line(i + 1)? == '"')
            and (line(i + 2)? == '"'))
        end
      end
      i = i + 1
    end
    false

  fun _is_ident_char(ch: U8): Bool =>
    """
    Check if a character can appear in a Pony identifier.
    """
    ((ch >= 'a') and (ch <= 'z'))
      or ((ch >= 'A') and (ch <= 'Z'))
      or ((ch >= '0') and (ch <= '9'))
      or (ch == '_')

  fun _is_block_opener(word: String val): Bool =>
    """
    Check if a word is a keyword that opens a block terminated by `end`.
    """
    (word == "if") or (word == "ifdef") or (word == "iftype")
      or (word == "while") or (word == "for") or (word == "try")
      or (word == "match") or (word == "repeat") or (word == "with")
      or (word == "recover") or (word == "object")
