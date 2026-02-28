use ast = "pony_compiler"

primitive TypeParameterFormat is ASTRule
  """
  Checks formatting of multiline type parameter declarations per the style
  guide.

  When type parameters span multiple lines, checks:
  - The opening '[' is on the same line as the entity or method name.
  - Each type parameter is on its own line (no two sharing a line).
  - For entities with a provides clause ('is'), the 'is' keyword is at the
    entity keyword's column + 2 when it appears as the first non-whitespace
    on its line.

  Single-line type parameter lists are not checked (except for the '['
  same-line requirement, which always applies).
  """
  fun id(): String val => "style/type-parameter-format"
  fun category(): String val => "style"

  fun description(): String val =>
    "multiline type parameter formatting"
      + " (bracket placement, layout, and 'is' alignment)"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [ ast.TokenIds.tk_class()
      ast.TokenIds.tk_actor()
      ast.TokenIds.tk_primitive()
      ast.TokenIds.tk_struct()
      ast.TokenIds.tk_trait()
      ast.TokenIds.tk_interface()
      ast.TokenIds.tk_type()
      ast.TokenIds.tk_fun()
      ast.TokenIds.tk_new()
      ast.TokenIds.tk_be() ]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check formatting of type parameter declarations.
    """
    let token_id = node.id()
    let is_method = _is_method(token_id)

    // Child indices: entities have name at 0, typeparams at 1;
    // methods have name at 1, typeparams at 2.
    let name_idx: USize = if is_method then 1 else 0 end
    let tp_idx: USize = if is_method then 2 else 1 end

    let name_node =
      try node(name_idx)?
      else return recover val Array[Diagnostic val] end
      end
    let typeparams =
      try node(tp_idx)?
      else return recover val Array[Diagnostic val] end
      end

    // No type parameters — nothing to check
    if typeparams.id() == ast.TokenIds.tk_none() then
      return recover val Array[Diagnostic val] end
    end

    let diags = recover iso Array[Diagnostic val] end

    // Check: '[' must be on the same line as the name.
    // For TK_TYPEPARAMS, the position may reflect the '['. If it doesn't,
    // fall back to the first child's line for the multiline check.
    let tp_line = _typeparams_line(typeparams)
    let name_line = name_node.line()
    if (tp_line > 0) and (tp_line != name_line) then
      diags.push(Diagnostic(
        id(),
        "'[' should be on the same line as the name",
        source.rel_path,
        tp_line,
        typeparams.pos()))
      // Further checks are meaningless if '[' is misplaced
      return consume diags
    end

    // Determine if multiline
    let num_tps = typeparams.num_children()
    if num_tps > 1 then
      try
        let first_tp = typeparams(0)?
        let first_line = _typeparam_line(first_tp)
        let last_line = _max_typeparam_line(typeparams)
        if first_line != last_line then
          // Multiline: check each is on its own line
          var prev_line = first_line
          var i: USize = 1
          while i < num_tps do
            let tp_node = typeparams(i)?
            let tp_l = _typeparam_line(tp_node)
            if tp_l == prev_line then
              diags.push(Diagnostic(
                id(),
                "each type parameter should be on its own line"
                  + " in a multiline declaration",
                source.rel_path,
                tp_l,
                tp_node.pos()))
            end
            prev_line = tp_l
            i = i + 1
          end
        end
      end
    end

    // Check 'is' constraint alignment (entities/type aliases only)
    if not is_method then
      // Only check when type params are multiline
      let multiline =
        try
          let first_tp = typeparams(0)?
          let first_line = _typeparam_line(first_tp)
          let last_line = _max_typeparam_line(typeparams)
          first_line != last_line
        else
          false
        end
      if multiline then
        try
          let provides = node(3)?
          if provides.id() != ast.TokenIds.tk_none() then
            let prov_line = provides.line()
            let keyword_col = node.pos()
            try
              let line_text = source.lines(prov_line - 1)?
              (let word, let word_col) =
                _first_nonws_word(line_text)
              // Verify it's actually 'is' (not 'interface', 'if', etc.)
              if word == "is" then
                let expected_col = keyword_col + 2
                if word_col != expected_col then
                  diags.push(Diagnostic(
                    id(),
                    "'is' should align at column "
                      + expected_col.string()
                      + " (indented from '"
                      + _keyword_name(token_id) + "')",
                    source.rel_path,
                    prov_line,
                    word_col))
                end
              end
            end
          end
        end
      end
    end

    consume diags

  fun _is_method(token_id: ast.TokenId): Bool =>
    """
    Check if the token represents a method declaration.
    """
    (token_id == ast.TokenIds.tk_fun())
      or (token_id == ast.TokenIds.tk_new())
      or (token_id == ast.TokenIds.tk_be())

  fun _keyword_name(token_id: ast.TokenId): String val =>
    """
    Return the source keyword for the given token type.
    """
    if token_id == ast.TokenIds.tk_class() then "class"
    elseif token_id == ast.TokenIds.tk_actor() then "actor"
    elseif token_id == ast.TokenIds.tk_primitive() then "primitive"
    elseif token_id == ast.TokenIds.tk_struct() then "struct"
    elseif token_id == ast.TokenIds.tk_trait() then "trait"
    elseif token_id == ast.TokenIds.tk_interface() then "interface"
    elseif token_id == ast.TokenIds.tk_type() then "type"
    elseif token_id == ast.TokenIds.tk_fun() then "fun"
    elseif token_id == ast.TokenIds.tk_new() then "new"
    elseif token_id == ast.TokenIds.tk_be() then "be"
    else "unknown"
    end

  fun _typeparams_line(typeparams: ast.AST box): USize =>
    """
    Get the line of the type parameters node. Since TK_TYPEPARAMS is
    abstract, its position may not reflect the '['. Fall back to the
    first child's line if the node's own line is 0.
    """
    let l = typeparams.line()
    if l > 0 then return l end
    try _typeparam_line(typeparams(0)?)
    else 0
    end

  fun _typeparam_line(tp: ast.AST box): USize =>
    """
    Get the line number of a type parameter node. Since TK_TYPEPARAM is
    abstract, use the first child's line (the type parameter name).
    """
    try tp(0)?.line()
    else tp.line()
    end

  fun _max_typeparam_line(typeparams: ast.AST box): USize =>
    """
    Find the maximum line number across all type parameter children.
    """
    var max_line: USize = 0
    var i: USize = 0
    let count = typeparams.num_children()
    while i < count do
      try
        let tp = typeparams(i)?
        let l = _typeparam_line(tp)
        if l > max_line then max_line = l end
      end
      i = i + 1
    end
    max_line

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
        if (ch == ' ') or (ch == '\t') or (ch == '(') or (ch == '[') then
          break
        end
      end
      i = i + 1
    end

    (recover val line.substring(
      ISize.from[USize](start),
      ISize.from[USize](i)) end, col)
