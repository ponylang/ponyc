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
    Extracts the default value string from a parameter's default AST
    node.
    """
    if def_val.id() != ast.TokenIds.tk_none() then
      try _unparse_expr(def_val(0)?) end
    end

  fun _unparse_expr(node: ast.AST box): (String | None) =>
    """
    Converts an expression AST node into its source-level string
    representation.
    """
    let id = node.id()
    if id == ast.TokenIds.tk_int() then
      node.get_print()
    elseif id == ast.TokenIds.tk_float() then
      node.get_print()
    elseif id == ast.TokenIds.tk_string() then
      "\"" + node.get_print() + "\""
    elseif id == ast.TokenIds.tk_true() then
      "true"
    elseif id == ast.TokenIds.tk_false() then
      "false"
    elseif id == ast.TokenIds.tk_this() then
      "this"
    elseif id == ast.TokenIds.tk_location() then
      "__loc"
    elseif id == ast.TokenIds.tk_reference() then
      _unparse_reference(node)
    elseif id == ast.TokenIds.tk_typeref() then
      _unparse_typeref(node)
    elseif id == ast.TokenIds.tk_dot() then
      _unparse_dot(node)
    elseif id == ast.TokenIds.tk_call() then
      _unparse_call(node)
    elseif id == ast.TokenIds.tk_qualify() then
      _unparse_qualify(node)
    elseif (id == ast.TokenIds.tk_minus_new()) or
      (id == ast.TokenIds.tk_unary_minus())
    then
      _unparse_unary_minus(node)
    elseif id == ast.TokenIds.tk_seq() then
      try _unparse_expr(node(0)?) end
    elseif (id == ast.TokenIds.tk_funref()) or
      (id == ast.TokenIds.tk_newref()) or
      (id == ast.TokenIds.tk_beref())
    then
      _unparse_member_ref(node)
    elseif id == ast.TokenIds.tk_nominal() then
      _unparse_nominal(node)
    elseif id == ast.TokenIds.tk_typeparamref() then
      try node(0)?.nice_name() end
    elseif id == ast.TokenIds.tk_typealiasref() then
      _unparse_type_alias_ref(node)
    elseif id == ast.TokenIds.tk_tuple() then
      _unparse_tuple(node)
    else
      node.get_print()
    end

  fun _unparse_reference(node: ast.AST box): (String | None) =>
    try (node(0)?.token_value() as String) end

  fun _unparse_typeref(node: ast.AST box): (String | None) =>
    """
    TK_TYPEREF children: [0] package, [1] id, [2] type_args.
    """
    try
      let name = node(1)?.nice_name()
      let targs = node(2)?
      if targs.id() != ast.TokenIds.tk_none() then
        let result = recover iso String end
        result.append(name)
        result.append("[")
        var first = true
        for child in targs.children() do
          if not first then result.append(", ") end
          first = false
          match _unparse_expr(child)
          | let s: String => result.append(s)
          end
        end
        result.append("]")
        consume result
      else
        name
      end
    end

  fun _unparse_dot(node: ast.AST box): (String | None) =>
    try
      let left = _unparse_expr(node(0)?) as String
      let right = (node(1)?.token_value() as String)
      left + "." + right
    end

  fun _unparse_call(node: ast.AST box): (String | None) =>
    """
    TK_CALL children: [0] receiver, [1] positional_args,
    [2] named_args, [3] partial.

    The sugar pass transforms `-literal` into `literal.neg()`, so
    this reconstructs the original form for neg calls on literals.
    """
    try
      let recv = node(0)?
      let pos_args_node = node(1)?
      if _is_desugared_neg(recv, pos_args_node) then
        let lit = recv(0)?
        let lit_str = _unparse_expr(lit) as String
        return "-" + lit_str
      end

      let receiver = _unparse_expr(node(0)?) as String
      let result = recover iso String end
      result.append(receiver)
      result.append("(")
      let pos_args = node(1)?
      if pos_args.id() != ast.TokenIds.tk_none() then
        var first = true
        for arg in pos_args.children() do
          if not first then result.append(", ") end
          first = false
          match _unparse_expr(arg)
          | let s: String => result.append(s)
          end
        end
      end
      let named_args = node(2)?
      if named_args.id() != ast.TokenIds.tk_none() then
        var first_named = true
        for narg in named_args.children() do
          if not first_named then
            result.append(", ")
          elseif pos_args.id() != ast.TokenIds.tk_none() then
            result.append(" where ")
          else
            result.append("where ")
          end
          first_named = false
          try
            let arg_name =
              (narg(0)?.token_value() as String)
            result.append(arg_name)
            result.append(" = ")
            match _unparse_expr(narg(1)?)
            | let s: String => result.append(s)
            end
          end
        end
      end
      result.append(")")
      if node(3)?.id() == ast.TokenIds.tk_question() then
        result.append("?")
      end
      consume result
    end

  fun _is_desugared_neg(
    recv: ast.AST box,
    pos_args: ast.AST box)
    : Bool
  =>
    """
    Returns true when the call is `literal.neg()` with no
    arguments — the form the sugar pass produces from `-literal`.
    """
    if pos_args.id() != ast.TokenIds.tk_none() then
      return false
    end
    if recv.id() != ast.TokenIds.tk_dot() then
      return false
    end
    try
      let method_name =
        (recv(1)?.token_value() as String)
      if method_name != "neg" then return false end
      let operand = recv(0)?
      (operand.id() == ast.TokenIds.tk_int()) or
        (operand.id() == ast.TokenIds.tk_float())
    else
      false
    end

  fun _unparse_qualify(node: ast.AST box): (String | None) =>
    """
    TK_QUALIFY children: [0] receiver, [1] type_args.
    """
    try
      let receiver = _unparse_expr(node(0)?) as String
      let targs = node(1)?
      let result = recover iso String end
      result.append(receiver)
      result.append("[")
      var first = true
      for child in targs.children() do
        if not first then result.append(", ") end
        first = false
        match _unparse_expr(child)
        | let s: String => result.append(s)
        end
      end
      result.append("]")
      consume result
    end

  fun _unparse_unary_minus(node: ast.AST box): (String | None) =>
    try
      let operand = _unparse_expr(node(0)?) as String
      "-" + operand
    end

  fun _unparse_member_ref(node: ast.AST box): (String | None) =>
    """
    TK_FUNREF/TK_NEWREF/TK_BEREF children: [0] receiver, [1] name.
    After PassRefer these replace TK_DOT.
    """
    try
      let left = _unparse_expr(node(0)?) as String
      let right = (node(1)?.token_value() as String)
      left + "." + right
    end

  fun _unparse_nominal(node: ast.AST box): (String | None) =>
    """
    TK_NOMINAL children: [0] package, [1] id, [2] type_args,
    [3] cap, [4] ephemeral.
    """
    try
      let name = node(1)?.nice_name()
      let targs = node(2)?
      if targs.id() != ast.TokenIds.tk_none() then
        let result = recover iso String end
        result.append(name)
        result.append("[")
        var first = true
        for child in targs.children() do
          if not first then result.append(", ") end
          first = false
          match _unparse_expr(child)
          | let s: String => result.append(s)
          end
        end
        result.append("]")
        consume result
      else
        name
      end
    end

  fun _unparse_type_alias_ref(node: ast.AST box): (String | None) =>
    """
    TK_TYPEALIASREF children: [0] id, [1] type_args, [2] cap,
    [3] ephemeral.
    """
    try
      let name = node(0)?.nice_name()
      let targs = node(1)?
      if targs.id() != ast.TokenIds.tk_none() then
        let result = recover iso String end
        result.append(name)
        result.append("[")
        var first = true
        for child in targs.children() do
          if not first then result.append(", ") end
          first = false
          match _unparse_expr(child)
          | let s: String => result.append(s)
          end
        end
        result.append("]")
        consume result
      else
        name
      end
    end

  fun _unparse_tuple(node: ast.AST box): (String | None) =>
    """
    TK_TUPLE children: sequence of elements.
    """
    let result = recover iso String end
    result.append("(")
    var first = true
    for child in node.children() do
      if not first then result.append(", ") end
      first = false
      match _unparse_expr(child)
      | let s: String => result.append(s)
      end
    end
    result.append(")")
    consume result
