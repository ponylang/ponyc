use ast = "pony_compiler"

primitive DocstringLeadingBlank is ASTRule
  """
  Flags docstrings where the opening `\"\"\"` is immediately followed by a
  blank line.

  A leading blank line wastes vertical space and can confuse the
  `style/blank-lines` rule's entity-boundary counting. The first line of
  content should begin on the line immediately after the opening `\"\"\"`.

  Types and methods annotated with `\nodoc\` are exempt, as are methods
  inside `\nodoc\`-annotated entities — consistent with
  `style/docstring-format`.
  """
  fun id(): String val => "style/docstring-leading-blank"
  fun category(): String val => "style"

  fun description(): String val =>
    "no blank line after opening \"\"\""

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
    Check that a docstring does not have a blank line immediately after
    the opening `\"\"\"`. Skips `\nodoc\`-annotated nodes and methods
    inside `\nodoc\`-annotated entities.
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

    // We have a docstring — check for a leading blank line
    let start_line = doc_node.line()
    if start_line == 0 then
      return recover val Array[Diagnostic val] end
    end

    _check_leading_blank(source, start_line)

  fun _check_leading_blank(source: SourceFile val, start_line: USize)
    : Array[Diagnostic val] val
  =>
    """
    Check whether the line after the opening `\"\"\"` at `start_line`
    (1-based) is blank. Only fires when the opening `\"\"\"` is on its
    own line (multi-line docstring).
    """
    let open_idx = start_line - 1
    let open_line =
      try source.lines(open_idx)?
      else return recover val Array[Diagnostic val] end
      end

    // Only check multi-line docstrings where """ is on its own line
    if not _is_only_triple_quote(open_line) then
      return recover val Array[Diagnostic val] end
    end

    // Check if the next line is blank
    try
      let next_line = source.lines(open_idx + 1)?
      if _is_blank(next_line) then
        return recover val
          [ Diagnostic(
            id(),
            "no blank line after opening \"\"\"",
            source.rel_path,
            start_line + 1,
            1)]
        end
      end
    end
    recover val Array[Diagnostic val] end

  fun _is_only_triple_quote(line: String val): Bool =>
    """
    Check if a line contains only whitespace and a single `\"\"\"`.
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
          if found_quote then return false end
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

  fun _is_blank(line: String val): Bool =>
    """
    Check if a line is blank (empty or all whitespace).
    """
    var i: USize = 0
    while i < line.size() do
      try
        let ch = line(i)?
        if (ch != ' ') and (ch != '\t') then return false end
      end
      i = i + 1
    end
    true
