use ast = "pony_compiler"

primitive DocstringFormat is ASTRule
  """
  Flags docstrings whose `\"\"\"` delimiters are not on their own lines.

  The Pony style guide requires multi-line docstrings with opening and closing
  `\"\"\"` each on a separate line. Single-line docstrings like
  `\"\"\"text\"\"\"` are flagged.

  Types and methods annotated with `\nodoc\` are exempt — their "docstrings"
  serve as inline comments (especially in test classes) and need not follow
  documentation formatting conventions. Methods inside `\nodoc\`-annotated
  entities are also exempt.
  """
  fun id(): String val => "style/docstring-format"
  fun category(): String val => "style"

  fun description(): String val =>
    "docstring \"\"\" tokens should be on their own lines"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [
      ast.TokenIds.tk_class(); ast.TokenIds.tk_actor()
      ast.TokenIds.tk_primitive(); ast.TokenIds.tk_struct()
      ast.TokenIds.tk_trait(); ast.TokenIds.tk_interface()
      ast.TokenIds.tk_fun(); ast.TokenIds.tk_new(); ast.TokenIds.tk_be()
    ]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check that a docstring's opening and closing `\"\"\"` delimiters are each
    on their own line. Skips `\nodoc\`-annotated nodes and methods inside
    `\nodoc\`-annotated entities.
    """
    let token_id = node.id()
    let is_method =
      (token_id == ast.TokenIds.tk_fun())
        or (token_id == ast.TokenIds.tk_new())
        or (token_id == ast.TokenIds.tk_be())

    // Skip \nodoc\-annotated nodes
    if node.has_annotation("nodoc") then
      return recover val Array[Diagnostic val] end
    end

    if is_method then
      // Skip methods inside \nodoc\-annotated entities
      try
        let entity =
          (node.parent() as ast.AST).parent() as ast.AST
        if entity.has_annotation("nodoc") then
          return recover val Array[Diagnostic val] end
        end
      end
    end

    // Find the docstring node
    let doc_node: ast.AST box =
      if is_method then
        // Check child 7 first (abstract method docstring), then body(0)
        let found =
          try
            let c7 = node(7)?
            if c7.id() == ast.TokenIds.tk_string() then
              c7
            else
              try
                let body = node(6)?
                if body.id() != ast.TokenIds.tk_none() then
                  let b0 = body(0)?
                  if b0.id() == ast.TokenIds.tk_string() then
                    b0
                  else
                    return recover val Array[Diagnostic val] end
                  end
                else
                  return recover val Array[Diagnostic val] end
                end
              else
                return recover val Array[Diagnostic val] end
              end
            end
          else
            return recover val Array[Diagnostic val] end
          end
        found
      else
        // Entity types: docstring at child 6
        try
          let c6 = node(6)?
          if c6.id() == ast.TokenIds.tk_none() then
            return recover val Array[Diagnostic val] end
          end
          c6
        else
          return recover val Array[Diagnostic val] end
        end
      end

    // We have a docstring — check its format
    let start_line = doc_node.line()
    if start_line == 0 then
      return recover val Array[Diagnostic val] end
    end

    _check_format(source, start_line)

  fun _check_format(source: SourceFile val, start_line: USize)
    : Array[Diagnostic val] val
  =>
    """
    Check that the opening and closing `\"\"\"` of a docstring at
    `start_line` (1-based) are each on their own line.

    The Pony parser rejects content on the opening line of a multi-line
    triple-quoted string, so the only reachable opening violation is a
    single-line docstring (`\"\"\"text\"\"\"`).
    """
    // Get the opening line (0-based index)
    let open_idx = start_line - 1
    let open_line =
      try
        source.lines(open_idx)?
      else
        return recover val Array[Diagnostic val] end
      end

    // Find position of opening """ on the line
    match _find_triple_quote(open_line)
    | let pos: USize =>
      // Check if there's non-whitespace content after the opening """
      let after = pos + 3
      if _has_non_whitespace_after(open_line, after) then
        // Single-line docstring """text""" — the parser prevents
        // content on the opening line of a multi-line string, so
        // this is the only reachable case.
        recover val
          [Diagnostic(id(),
            "opening \"\"\" should be on its own line",
            source.rel_path, start_line, 1)]
        end
      else
        // Opening """ is on its own line — check closing
        match _find_closing_line(source, open_idx + 1)
        | let close_line_idx: USize =>
          try
            let close_line = source.lines(close_line_idx)?
            if not _is_only_triple_quote(close_line) then
              return recover val
                [Diagnostic(id(),
                  "closing \"\"\" should be on its own line",
                  source.rel_path, close_line_idx + 1, 1)]
              end
            end
          end
        end
        recover val Array[Diagnostic val] end
      end
    else
      recover val Array[Diagnostic val] end
    end

  fun _find_triple_quote(line: String val): (USize | None) =>
    """
    Find the byte position of the first `\"\"\"` on a line.
    """
    _find_triple_quote_after(line, 0)

  fun _find_triple_quote_after(line: String val, start: USize)
    : (USize | None)
  =>
    """
    Find the byte position of a `\"\"\"` on a line, starting at `start`.
    """
    var i = start
    let size = line.size()
    while (i + 3) <= size do
      try
        if (line(i)? == '"') and (line(i + 1)? == '"')
          and (line(i + 2)? == '"')
        then
          return i
        end
      end
      i = i + 1
    end
    None

  fun _has_non_whitespace_after(line: String val, start: USize): Bool =>
    """
    Check if there is non-whitespace content after position `start`.
    """
    var i = start
    while i < line.size() do
      try
        let ch = line(i)?
        if (ch != ' ') and (ch != '\t') then return true end
      end
      i = i + 1
    end
    false

  fun _find_closing_line(source: SourceFile val, from_idx: USize)
    : (USize | None)
  =>
    """
    Scan source lines starting at `from_idx` (0-based) to find the line
    containing the closing `\"\"\"`.
    """
    var i = from_idx
    while i < source.lines.size() do
      try
        let line = source.lines(i)?
        match _find_triple_quote(line)
        | let _: USize => return i
        end
      end
      i = i + 1
    end
    None

  fun _is_only_triple_quote(line: String val): Bool =>
    """
    Check if a line contains only whitespace and a single `\"\"\"`,
    with nothing else (i.e., the closing delimiter is on its own line).
    """
    var found_quote = false
    var i: USize = 0
    let size = line.size()
    while i < size do
      try
        let ch = line(i)?
        if (ch == '"') and ((i + 3) <= size)
          and (line(i + 1)? == '"') and (line(i + 2)? == '"')
        then
          if found_quote then
            // Two sets of """ on one line
            return false
          end
          found_quote = true
          i = i + 3
        elseif (ch != ' ') and (ch != '\t') then
          return false
        else
          i = i + 1
        end
      else
        i = i + 1
      end
    end
    found_quote
