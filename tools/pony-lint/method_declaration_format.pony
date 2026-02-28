use ast = "pony_compiler"

primitive MethodDeclarationFormat is ASTRule
  """
  Checks formatting of multiline method declarations per the style guide.

  When a method's parameters span multiple lines, checks:
  - Each parameter is on its own line (no two parameters sharing a line).

  When a method declaration spans multiple lines, checks:
  - The return type ':' is at the method keyword's column + 2 (one indent
    level deeper) when it appears as the first non-whitespace on its line.
  - The '=>' is at the method keyword's column when it appears as the first
    non-whitespace on its line.

  Single-line method declarations are not checked.
  """
  fun id(): String val => "style/method-declaration-format"
  fun category(): String val => "style"

  fun description(): String val =>
    "multiline method declaration formatting"
      + " (parameter layout, return type and '=>' alignment)"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [ ast.TokenIds.tk_fun()
      ast.TokenIds.tk_new()
      ast.TokenIds.tk_be() ]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check formatting of a multiline method declaration.
    """
    let keyword_line = node.line()
    let keyword_col = node.pos()

    // Determine the keyword name for diagnostic messages
    let keyword_name = _keyword_name(node.id())

    let diags = recover iso Array[Diagnostic val] end

    // Check params (child 3)
    try
      let params = node(3)?
      if params.id() != ast.TokenIds.tk_none() then
        let num_params = params.num_children()
        if num_params > 1 then
          // Find line of first and last param to determine if multiline
          try
            let first_param = params(0)?
            let first_line = _param_line(first_param)
            let last_line = _last_param_line(params)
            if first_line != last_line then
              // Multiline params: check each is on its own line
              var prev_line = first_line
              var i: USize = 1
              while i < num_params do
                let param_node = params(i)?
                let param_l = _param_line(param_node)
                if param_l == prev_line then
                  diags.push(Diagnostic(
                    id(),
                    "each parameter should be on its own line"
                      + " in a multiline declaration",
                    source.rel_path,
                    param_l,
                    param_node.pos()))
                end
                prev_line = param_l
                i = i + 1
              end
            end
          end
        end
      end
    end

    // Check return type ':' alignment (child 4)
    try
      let ret_type = node(4)?
      if ret_type.id() != ast.TokenIds.tk_none() then
        let ret_line = ret_type.line()
        if ret_line > keyword_line then
          try
            let line_text = source.lines(ret_line - 1)?
            (let first_ch, let first_col) = _first_nonws_char(line_text)
            if first_ch == ':' then
              let expected_col = keyword_col + 2
              let actual_col = first_col + 1 // convert 0-based to 1-based
              if actual_col != expected_col then
                diags.push(Diagnostic(
                  id(),
                  "':' should align at column "
                    + expected_col.string()
                    + " (indented from '" + keyword_name + "')",
                  source.rel_path,
                  ret_line,
                  actual_col))
              end
            end
          end
        end
      end
    end

    // Check '=>' alignment (child 6 = body)
    try
      let body = node(6)?
      if body.id() != ast.TokenIds.tk_none() then
        let body_line = body.line()
        if body_line > keyword_line then
          // Scan backward from the line before the body to find '=>'
          var scan_line = body_line - 1
          while scan_line > keyword_line do
            try
              let line_text = source.lines(scan_line - 1)?
              (let word, let word_col) =
                _first_nonws_word(line_text)
              if word == "=>" then
                let actual_col = word_col
                if actual_col != keyword_col then
                  diags.push(Diagnostic(
                    id(),
                    "'=>' should align with '" + keyword_name
                      + "' keyword (column "
                      + keyword_col.string() + ")",
                    source.rel_path,
                    scan_line,
                    actual_col))
                end
                break
              end
              // Stop scanning if we hit a non-blank line that isn't '=>'
              if word.size() > 0 then break end
            end
            scan_line = scan_line - 1
          end
        end
      end
    end

    consume diags

  fun _keyword_name(token_id: ast.TokenId): String val =>
    """
    Return the source keyword for the given method token type.
    """
    if token_id == ast.TokenIds.tk_fun() then "fun"
    elseif token_id == ast.TokenIds.tk_new() then "new"
    elseif token_id == ast.TokenIds.tk_be() then "be"
    else "unknown"
    end

  fun _param_line(param: ast.AST box): USize =>
    """
    Get the line number of a parameter node. Since TK_PARAM is abstract
    (no source position), use the first child's line (the parameter name
    or dontcare).
    """
    try param(0)?.line()
    else param.line()
    end

  fun _last_param_line(params: ast.AST box): USize =>
    """
    Find the maximum line number across all parameter children.
    """
    var max_line: USize = 0
    var i: USize = 0
    let count = params.num_children()
    while i < count do
      try
        let p = params(i)?
        let l = _param_line(p)
        if l > max_line then max_line = l end
      end
      i = i + 1
    end
    max_line

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
        if (ch == ' ') or (ch == '\t') then break end
      end
      i = i + 1
    end

    (recover val line.substring(
      ISize.from[USize](start),
      ISize.from[USize](i)) end, col)
