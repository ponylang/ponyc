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
    AST node by reading directly from the module's source buffer.
    """
    if def_val.id() == ast.TokenIds.tk_none() then
      return None
    end
    try
      let expr = def_val(0)?
      let src = (expr.source_contents() as String box)
      (let start_pos, _) = expr.span()
      let start =
        _line_col_to_offset(src, start_pos.line(), start_pos.column())?
      let stop = _scan_expr_end(src, start)?
      recover val src.substring(start.isize(), stop.isize()) end
    end

  fun _line_col_to_offset(
    src: String box,
    line: USize,
    col: USize)
    : USize ?
  =>
    """
    Converts a 1-based line and column to a 0-based byte offset.
    """
    var offset: USize = 0
    var cur_line: USize = 1
    while cur_line < line do
      while src(offset)? != '\n' do
        offset = offset + 1
      end
      offset = offset + 1
      cur_line = cur_line + 1
    end
    offset + (col - 1)

  fun _scan_expr_end(
    src: String box,
    start: USize)
    : USize ?
  =>
    """
    Scans forward from `start` through balanced delimiters to find
    where the expression ends. Stops at `,` or `)` at depth zero,
    which are the parameter-list terminators that follow a default
    value.
    """
    var i = start
    var depth: USize = 0
    var in_string = false
    var in_triple = false
    let len = src.size()

    while i < len do
      let c = src(i)?

      if in_triple then
        if (c == '"') and
          ((i + 2) < len) and
          (src(i + 1)? == '"') and
          (src(i + 2)? == '"')
        then
          in_triple = false
          in_string = false
          i = i + 3
          continue
        end
        i = i + 1
        continue
      end

      if in_string then
        if c == '\\' then
          i = i + 2
          continue
        end
        if c == '"' then
          in_string = false
        end
        i = i + 1
        continue
      end

      match c
      | '"' =>
        if ((i + 2) < len) and
          (src(i + 1)? == '"') and
          (src(i + 2)? == '"')
        then
          in_triple = true
          in_string = true
          i = i + 3
          continue
        end
        in_string = true
        i = i + 1
        continue
      | '(' | '[' =>
        depth = depth + 1
      | ')' =>
        if depth == 0 then return i end
        depth = depth - 1
      | ']' =>
        if depth > 0 then depth = depth - 1 end
      | ',' =>
        if depth == 0 then return i end
      end
      i = i + 1
    end
    error
