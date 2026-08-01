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
    The default value of a parameter, rendered as source.

    Literals come straight from `get_print()`. Anything built from other
    nodes — a call, a field access, a negated literal — is reassembled from
    the AST by `_render_expr`, because `get_print()` on such a node returns
    the token's own name: `USize.max_value()` came out as `call`.
    """
    if def_val.id() != ast.TokenIds.tk_none() then
      try
        _render_expr(def_val(0)?)
      end
    end

  fun _render_expr(node: ast.AST box): String =>
    """
    An expression AST rendered back to source text.

    Covers what appears in a default value: literals, references, field and
    method access, calls with their arguments, and qualified type arguments.
    A node this doesn't know falls back to `get_print()`, which is right for
    every leaf token and no worse than the previous behaviour for the rest.
    """
    let id = node.id()
    if id == ast.TokenIds.tk_string() then
      "\"" + node.get_print() + "\""
    elseif id == ast.TokenIds.tk_dot() then
      try
        _render_expr(node(0)?) + "." + node(1)?.get_print()
      else
        node.get_print()
      end
    elseif id == ast.TokenIds.tk_call() then
      try
        let receiver = node(0)?
        // `-1` reaches the AST as a call to `neg` on the literal. Write the
        // minus back rather than exposing the desugaring — and without the
        // empty argument list a plain call would print.
        if
          (receiver.id() == ast.TokenIds.tk_dot()) and
          (receiver(1)?.get_print() == "neg")
        then
          "-" + _render_expr(receiver(0)?)
        else
          _render_expr(receiver) + "(" + _render_args(node(1)?) + ")"
        end
      else
        node.get_print()
      end
    elseif id == ast.TokenIds.tk_qualify() then
      try
        _render_expr(node(0)?) + "[" + _render_args(node(1)?) + "]"
      else
        node.get_print()
      end
    elseif (id == ast.TokenIds.tk_reference()) or (id == ast.TokenIds.tk_seq())
    then
      try _render_expr(node(0)?) else node.get_print() end
    else
      node.get_print()
    end

  fun _render_args(args: ast.AST box): String =>
    """
    Argument list rendered as source, comma-separated. An absent list
    (`TK_NONE`) renders as the empty string, which is what a call with no
    arguments needs.
    """
    if args.id() == ast.TokenIds.tk_none() then
      ""
    else
      let result = String
      var first = true
      for arg in args.children() do
        if not first then result.append(", ") end
        result.append(_render_expr(arg))
        first = false
      end
      result.clone()
    end
