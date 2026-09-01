use "collections"
use ast = "pony_compiler"

primitive _TypeExtractor
  """
  Converts an AST type node into a `DocType` IR value.

  The `package_map` parameter maps package AST pointer addresses to
  qualified names, used for resolving nominal type TQFNs via
  `definitions()`.
  """
  fun apply(
    type_ast: ast.AST box,
    package_map: Map[USize, String] val)
    : DocType
  =>
    match type_ast.id()
    | ast.TokenIds.tk_nominal() => _nominal(type_ast, package_map)
    | ast.TokenIds.tk_typealiasref() =>
      _type_alias_ref(type_ast, package_map)
    | ast.TokenIds.tk_typeparamref() => _type_param_ref(type_ast)
    | ast.TokenIds.tk_uniontype() =>
      _type_list(
        type_ast,
        package_map,
        {(types) => DocUnion(types) })
    | ast.TokenIds.tk_isecttype() =>
      _type_list(
        type_ast,
        package_map,
        {(types) => DocIntersection(types) })
    | ast.TokenIds.tk_tupletype() =>
      _type_list(
        type_ast,
        package_map,
        {(types) => DocTuple(types) })
    | ast.TokenIds.tk_arrow() => _arrow(type_ast, package_map)
    | ast.TokenIds.tk_thistype() => DocThis
    | ast.TokenIds.tk_iso() => DocCapability(type_ast.get_print())
    | ast.TokenIds.tk_trn() => DocCapability(type_ast.get_print())
    | ast.TokenIds.tk_ref() => DocCapability(type_ast.get_print())
    | ast.TokenIds.tk_val() => DocCapability(type_ast.get_print())
    | ast.TokenIds.tk_box() => DocCapability(type_ast.get_print())
    | ast.TokenIds.tk_tag() => DocCapability(type_ast.get_print())
    else
      // Unhandled token — should never happen in practice.
      // In practice, TK_LAMBDATYPE, TK_BARELAMBDATYPE, TK_FUNTYPE,
      // and TK_DONTCARETYPE never appear at this AST level.
      _Unreachable()
      DocCapability("unknown")
    end

  fun _nominal(
    type_ast: ast.AST box,
    package_map: Map[USize, String] val)
    : DocNominal
  =>
    """
    Extracts a `DocNominal` from a TK_NOMINAL AST node.

    Child access (post-REORDER): [0] package, [1] id, [2] type_args,
    [3] cap, [4] ephemeral.
    """
    try
      let id_node = type_ast(1)?

      // Display name uses nice_name (handles hygienic aliases).
      // Raw name uses token_value for private/anonymous detection.
      let display_name = id_node.nice_name()
      let raw_name = (id_node.token_value() as String)

      // Type args
      let targs_node = type_ast(2)?
      let args: Array[DocType] iso = recover iso Array[DocType] end
      for child in targs_node.children() do
        if child.id() != ast.TokenIds.tk_none() then
          args.push(apply(child, package_map))
        end
      end
      let type_args: Array[DocType] val = consume args

      // Cap (via doc_get_cap logic)
      let cap_node = type_ast(3)?
      let cap = _get_cap(cap_node)

      // Ephemeral
      let eph_node = type_ast(4)?
      let ephemeral: (String | None) =
        if eph_node.id() != ast.TokenIds.tk_none() then
          eph_node.get_print()
        else
          None
        end

      // Resolve TQFN: find definition, walk to TK_PACKAGE, look up name
      let tqfn = _resolve_tqfn(type_ast, package_map)

      DocNominal(
        display_name,
        tqfn,
        type_args,
        cap,
        ephemeral,
        Filter.is_private(raw_name),
        Filter.is_internal(raw_name))
    else
      DocNominal(
        "unknown",
        "",
        recover val Array[DocType] end,
        None,
        None,
        false,
        false)
    end

  fun _type_alias_ref(
    type_ast: ast.AST box,
    package_map: Map[USize, String] val)
    : DocNominal
  =>
    """
    Extracts a `DocNominal` from a TK_TYPEALIASREF AST node.

    Child access: [0] id, [1] type_args, [2] cap, [3] ephemeral.
    Unlike TK_NOMINAL, there is no package child so indices are shifted.
    """
    try
      let id_node = type_ast(0)?

      let display_name = id_node.nice_name()
      let raw_name = (id_node.token_value() as String)

      // Type args
      let targs_node = type_ast(1)?
      let args: Array[DocType] iso = recover iso Array[DocType] end
      for child in targs_node.children() do
        if child.id() != ast.TokenIds.tk_none() then
          args.push(apply(child, package_map))
        end
      end
      let type_args: Array[DocType] val = consume args

      // Cap
      let cap_node = type_ast(2)?
      let cap = _get_cap(cap_node)

      // Ephemeral
      let eph_node = type_ast(3)?
      let ephemeral: (String | None) =
        if eph_node.id() != ast.TokenIds.tk_none() then
          eph_node.get_print()
        else
          None
        end

      // Resolve TQFN via definition pointer
      let tqfn = _resolve_tqfn(type_ast, package_map)

      DocNominal(
        display_name,
        tqfn,
        type_args,
        cap,
        ephemeral,
        Filter.is_private(raw_name),
        Filter.is_internal(raw_name))
    else
      DocNominal(
        "unknown",
        "",
        recover val Array[DocType] end,
        None,
        None,
        false,
        false)
    end

  fun _type_param_ref(type_ast: ast.AST box): DocTypeParamRef =>
    """
    Extracts a `DocTypeParamRef` from a TK_TYPEPARAMREF AST node.

    Child access (post-REORDER): [0] id, [1] cap, [2] ephemeral.
    """
    try
      let id_node = type_ast(0)?
      let name = id_node.nice_name()

      let eph_node = type_ast(2)?
      let ephemeral: (String | None) =
        if eph_node.id() != ast.TokenIds.tk_none() then
          eph_node.get_print()
        else
          None
        end

      DocTypeParamRef(name, ephemeral)
    else
      DocTypeParamRef("unknown", None)
    end

  fun _type_list(
    type_ast: ast.AST box,
    package_map: Map[USize, String] val,
    builder: {(Array[DocType] val): DocType} val)
    : DocType
  =>
    """
    Extracts child types and wraps them via the builder
    function.
    """
    let result: Array[DocType] iso = recover iso Array[DocType] end
    for child in type_ast.children() do
      result.push(apply(child, package_map))
    end
    let types: Array[DocType] val = consume result
    builder(types)

  fun _arrow(
    type_ast: ast.AST box,
    package_map: Map[USize, String] val)
    : DocArrow
  =>
    """
    Extracts a `DocArrow` from a TK_ARROW AST node.

    Child access (post-REORDER): [0] left, [1] right.
    """
    try
      let left = apply(type_ast(0)?, package_map)
      let right = apply(type_ast(1)?, package_map)
      DocArrow(left, right)
    else
      DocArrow(DocThis, DocThis)
    end

  fun _resolve_tqfn(
    type_ast: ast.AST box,
    package_map: Map[USize, String] val)
    : String
  =>
    """
    Resolves the TQFN for a nominal type by finding its definition,
    walking up to the containing TK_PACKAGE, and looking up the
    package's qualified name in the map.
    """
    try
      let defs = type_ast.definitions()
      let target = defs(0)?
      // Walk up to the TK_PACKAGE
      var parent = target.parent()
      while true do
        match parent
        | let p: ast.AST =>
          if p.id() == ast.TokenIds.tk_package() then
            let pkg_addr = p.raw.usize()
            let pkg_name = package_map(pkg_addr)?
            let type_name = (target(0)?.token_value() as String)
            return TQFN(pkg_name, type_name)
          end
          parent = p.parent()
        else
          break
        end
      end
      ""
    else
      ""
    end

  fun get_cap(cap_node: ast.AST box): (String | None) =>
    """
    Returns the capability string for a cap AST node.

    Handles TK_ISO through TK_TAG plus TK_CAP_READ, TK_CAP_SEND,
    TK_CAP_SHARE. Returns `None` for TK_NONE or unrecognized tokens.
    """
    _get_cap(cap_node)

  fun _get_cap(cap_node: ast.AST box): (String | None) =>
    match cap_node.id()
    | ast.TokenIds.tk_iso()
    | ast.TokenIds.tk_trn()
    | ast.TokenIds.tk_ref()
    | ast.TokenIds.tk_val()
    | ast.TokenIds.tk_box()
    | ast.TokenIds.tk_tag()
    | ast.TokenIds.tk_cap_read()
    | ast.TokenIds.tk_cap_send()
    | ast.TokenIds.tk_cap_share() =>
      cap_node.get_print()
    else
      None
    end

  fun get_default_value(def_val: ast.AST box): (String | None) =>
    """
    Extracts the default value source text from a parameter's default
    AST node by reading it from the original source. This preserves
    the user's spelling (e.g. `ISize.max_value()`, `-1`) regardless of
    how the compiler transforms the AST.

    Block constructs (`recover`, `if`, `object`, etc.) have a closing
    `end` keyword that the parser consumes without leaving an AST node.
    After extracting via `source_span()`, any unmatched block openers
    are balanced by scanning forward for matching `end` keywords.
    """
    if def_val.id() != ast.TokenIds.tk_none() then
      try
        let src = (def_val.source_contents() as String box)
        (let start_pos, let end_pos') = def_val.source_span()
        let start_offset = _pos_to_offset(src, start_pos)?
        var end_offset = _pos_to_offset(src, end_pos')?
        if end_offset >= start_offset then
          var extracted: String val =
            recover val
              src.substring(
                ISize.from[USize](start_offset),
                ISize.from[USize](end_offset + 1))
                .> strip()
            end

          // Block constructs lose their closing `end` in the AST.
          // Count unmatched openers and scan forward for each.
          var missing_ends = _count_unmatched_ends(extracted)
          while missing_ends > 0 do
            end_offset = _scan_to_next_end(src, end_offset + 1)?
            missing_ends = missing_ends - 1
          end

          if missing_ends == 0 then
            extracted =
              recover val
                src.substring(
                  ISize.from[USize](start_offset),
                  ISize.from[USize](end_offset + 1))
                  .> strip()
              end
          end
          extracted
        end
      end
    end

  fun _count_unmatched_ends(text: String val): USize =>
    """
    Count block-opening keywords minus `end` keywords. Returns the
    number of `end` keywords missing from the extracted text.
    """
    let openers =
      [ as String:
        "recover"; "if"; "ifdef"; "iftype"; "match"; "while"; "for"
        "repeat"; "object"; "lambda"; "try"]
    var opens: USize = 0
    var ends: USize = 0

    // Split on whitespace and count keyword tokens
    var i: USize = 0
    let size = text.size()
    while i < size do
      // Skip non-word characters
      try
        while (i < size) and not _is_word_char(text(i)?) do
          // Skip string literals
          if text(i)? == '"' then
            i = i + 1
            if ((i + 1) < size) and (text(i)? == '"') and
              (text(i + 1)? == '"')
            then
              // Triple-quoted string
              i = i + 2
              while (i + 2) < size do
                if (text(i)? == '"') and (text(i + 1)? == '"') and
                  (text(i + 2)? == '"')
                then
                  i = i + 3
                  break
                end
                i = i + 1
              end
            else
              while (i < size) and (text(i)? != '"') do
                i = i + 1
              end
              if i < size then i = i + 1 end
            end
          else
            i = i + 1
          end
        end
      end

      // Extract word
      let word_start = i
      try
        while (i < size) and _is_word_char(text(i)?) do
          i = i + 1
        end
      end

      if i > word_start then
        let word: String val =
          text.substring(
            ISize.from[USize](word_start),
            ISize.from[USize](i))
        if word == "end" then
          ends = ends + 1
        else
          for opener in openers.values() do
            if word == opener then
              opens = opens + 1
              break
            end
          end
        end
      end
    end

    if opens > ends then opens - ends else 0 end

  fun _is_word_char(c: U8): Bool =>
    ((c >= 'a') and (c <= 'z')) or
    ((c >= 'A') and (c <= 'Z')) or
    ((c >= '0') and (c <= '9')) or
    (c == '_')

  fun _scan_to_next_end(
    src: String box,
    from: USize)
    : USize ?
  =>
    """
    Scan forward from byte offset `from` to find the next `end` keyword
    (a standalone word, not a substring of another identifier). Returns
    the byte offset of the last character of that `end` token.
    """
    var i = from
    let size = src.size()
    while i < size do
      if not _is_word_char(src(i)?) then
        i = i + 1
      else
        let word_start = i
        while (i < size) and _is_word_char(src(i)?) do
          i = i + 1
        end
        if (src.substring(
          ISize.from[USize](word_start), ISize.from[USize](i)) == "end")
        then
          return i - 1
        end
      end
    end
    error

  fun _pos_to_offset(
    src: String box,
    p: ast.Position)
    : USize ?
  =>
    if p.line() == 1 then
      p.column() - 1
    else
      let line_idx = src.find("\n" where nth = p.line() - 2)?
      line_idx.usize() + p.column()
    end
