use "files"

primitive InfoBackend is Backend
  """
  Generates GNU Info documentation output.

  Produces a single `.info` file directly from the documentation IR —
  no Texinfo intermediate and no `makeinfo` dependency. The output is
  readable by any Info viewer (Emacs, standalone `info`, `pinfo`).
  """
  fun generate(
    program: DocProgram box,
    output_dir: FilePath,
    include_private: Bool)
    ?
  =>
    """
    Write `<program.name>.info` into `output_dir`.
    """
    let file_name: String val = program.name + ".info"

    let nodes = Array[(String val, String val)]

    // Top node
    let top_body = recover iso String(2048) end
    top_body.append(program.name)
    top_body.append("\n")
    top_body.append(_underline(program.name))
    top_body.append("\n\n* Menu:\n\n")
    for mod in program.modules.values() do
      let pkg_name = _pkg_node_name(mod.qualified_name)
      top_body.append("* ")
      top_body.append(pkg_name)
      top_body.append("::")
      top_body.append("\n")
    end
    nodes.push(("Top", consume top_body))

    // Package + entity nodes
    for mod in program.modules.values() do
      let pkg_name = _pkg_node_name(mod.qualified_name)

      let pkg_body = recover iso String(2048) end
      match mod.doc_string
      | let ds: String =>
        pkg_body.append(ds)
      else
        pkg_body.append("No module doc string provided for ")
        pkg_body.append(mod.qualified_name)
        pkg_body.append(".")
      end

      let menu_entries = Array[String val]
      for entity in mod.public_types.values() do
        menu_entries.push(
          _build_menu_entry(
            entity.tqfn, entity.kind.keyword(), entity.name))
      end
      if include_private then
        for entity in mod.private_types.values() do
          menu_entries.push(
            _build_menu_entry(
              entity.tqfn, entity.kind.keyword(), entity.name))
        end
      end

      if menu_entries.size() > 0 then
        pkg_body.append("\n\n* Menu:\n\n")
        for entry in menu_entries.values() do
          pkg_body.append(entry)
          pkg_body.append("\n")
        end
      end

      nodes.push((pkg_name, consume pkg_body))

      // Entity nodes for public types
      for entity in mod.public_types.values() do
        nodes.push(
          (entity.tqfn, _render_entity(entity, include_private)))
      end

      // Entity nodes for private types
      for entity in mod.private_types.values() do
        nodes.push(
          (entity.tqfn, _render_entity(entity, include_private)))
      end
    end

    // Type Index node
    let type_idx = recover iso String(2048) end
    type_idx.append("Type Index\n")
    type_idx.append(_underline("Type Index"))
    type_idx.append("\n\n")
    for mod in program.modules.values() do
      for entity in mod.public_types.values() do
        type_idx.append("\0\b[index\0\b]\n")
        type_idx.append("* ")
        type_idx.append(entity.tqfn)
        type_idx.append(":  *note ")
        type_idx.append(entity.name)
        type_idx.append(": ")
        type_idx.append(entity.tqfn)
        type_idx.append(".\n")
      end
      if include_private then
        for entity in mod.private_types.values() do
          type_idx.append("\0\b[index\0\b]\n")
          type_idx.append("* ")
          type_idx.append(entity.tqfn)
          type_idx.append(":  *note ")
          type_idx.append(entity.name)
          type_idx.append(": ")
          type_idx.append(entity.tqfn)
          type_idx.append(".\n")
        end
      end
    end
    nodes.push(("Type Index", consume type_idx))

    // Method Index node
    let meth_idx = recover iso String(2048) end
    meth_idx.append("Method Index\n")
    meth_idx.append(_underline("Method Index"))
    meth_idx.append("\n\n")
    for mod in program.modules.values() do
      for entity in mod.public_types.values() do
        meth_idx.append(
          _method_index_entries(entity, include_private))
      end
      if include_private then
        for entity in mod.private_types.values() do
          meth_idx.append(
            _method_index_entries(entity, include_private))
        end
      end
    end
    nodes.push(("Method Index", consume meth_idx))

    let preamble: String val =
      "This is " + file_name + ", produced by pony-doc.\n"

    let output = recover iso String(65536) end
    output.append(preamble)

    let tag_entries = Array[(String val, USize)]

    var i: USize = 0
    while i < nodes.size() do
      try
        (let node_name, let node_content) = nodes(i)?

        // Determine Next/Prev/Up
        let next: (String | None) =
          if (i + 1) < nodes.size()
          then try nodes(i + 1)?._1 else None end
          else None
          end

        let prev: (String | None) =
          if i > 0
          then try nodes(i - 1)?._1 else None end
          else None
          end

        let up: String =
          if i == 0 then
            "(dir)"
          elseif (node_name == "Type Index") or
              (node_name == "Method Index")
          then
            "Top"
          else
            _find_up(nodes, i, program)
          end

        // Record the byte offset of the \x1F separator
        tag_entries.push((node_name, output.size()))

        output.append("\x1F\n")
        output.append(
          _node_header(file_name, node_name, next, prev, up))
        output.append("\n")
        output.append(node_content)
        output.append("\n")
      else
        _Unreachable()
      end
      i = i + 1
    end

    // Tag table
    output.append(_build_tag_table(tag_entries))

    // Write the file
    let path = output_dir.join(file_name)?
    match CreateFile(path)
    | let file: File =>
      file.write(consume output)
      file.dispose()
    else
      error
    end

  fun _pkg_node_name(qualified_name: String): String =>
    """
    Produce a node name for a package from its qualified module name.
    Replaces `/` with `-` and appends `--index` so that a package node
    name can never collide with an entity TQFN.
    """
    let result = recover iso String(qualified_name.size() + 7) end
    for byte in qualified_name.values() do
      if byte == '/' then
        result.push('-')
      else
        result.push(byte)
      end
    end
    result.append("--index")
    consume result

  fun _node_header(
    file_name: String,
    node: String,
    next: (String | None),
    prev: (String | None),
    up: String)
    : String
  =>
    let result = recover iso String(256) end
    result.append("File: ")
    result.append(file_name)
    result.append(",  Node: ")
    result.append(node)
    match next
    | let n: String =>
      result.append(",  Next: ")
      result.append(n)
    end
    match prev
    | let p: String =>
      result.append(",  Prev: ")
      result.append(p)
    end
    result.append(",  Up: ")
    result.append(up)
    consume result

  fun _underline(title: String): String =>
    let result = recover iso String(title.size()) end
    var n: USize = 0
    while n < title.size() do
      result.push('*')
      n = n + 1
    end
    consume result

  fun _build_menu_entry(
    tqfn: String,
    kind: String,
    name: String)
    : String val
  =>
    let result = recover iso String(128) end
    result.append("* ")
    result.append(tqfn)
    result.append("::    ")
    result.append(kind)
    result.append(" ")
    result.append(name)
    consume result

  fun _render_entity(
    entity: DocEntity box,
    include_private: Bool)
    : String val
  =>
    let result = recover iso String(4096) end

    // Title with underline
    result.append(entity.name)
    result.append("\n")
    result.append(_underline(entity.name))
    result.append("\n\n")

    // Docstring
    match entity.doc_string
    | let ds: String =>
      result.append(ds)
      result.append("\n\n")
    end

    // Signature block (plain text, no links)
    result.append(entity.kind.keyword())
    result.append(" ")
    match entity.cap
    | let c: String =>
      result.append(c)
      result.append(" ")
    end
    result.append(entity.name)
    result.append(
      TypeRenderer.render_type_params(
        entity.type_params,
        None,
        true,
        include_private))
    result.append(
      TypeRenderer.render_provides(
        entity.provides,
        " is\n  ",
        ",\n  ",
        "",
        None,
        include_private))
    result.append("\n\n")

    // Provides/implements section with cross-references
    match entity.kind
    | EntityTypeAlias =>
      result.append(
        TypeRenderer.render_provides(
          entity.provides,
          "Type Alias For\n\n  ",
          "\n  ",
          "\n\n",
          InfoLinkFormat,
          include_private))
    else
      result.append(
        TypeRenderer.render_provides(
          entity.provides,
          "Implements\n\n  ",
          "\n  ",
          "\n\n",
          InfoLinkFormat,
          include_private))
    end

    // Member sections
    result.append(
      _render_methods(
        entity.constructors, "Constructors", include_private))
    result.append(
      _render_fields(
        entity.public_fields, "Public fields", include_private))
    result.append(
      _render_methods(
        entity.public_behaviours, "Public Behaviours", include_private))
    result.append(
      _render_methods(
        entity.public_functions, "Public Functions", include_private))
    result.append(
      _render_methods(
        entity.private_behaviours, "Private Behaviours", include_private))
    result.append(
      _render_methods(
        entity.private_functions, "Private Functions", include_private))

    consume result

  fun _render_methods(
    methods: Array[DocMethod] val,
    title: String,
    include_private: Bool)
    : String val
  =>
    if methods.size() == 0 then return "" end

    let result = recover iso String(2048) end
    result.append(title)
    result.append("\n")
    result.append(_underline(title))
    result.append("\n\n")

    for method in methods.values() do
      result.append(_render_method(method, include_private))
    end
    consume result

  fun _render_method(
    method: DocMethod box,
    include_private: Bool)
    : String val
  =>
    let result = recover iso String(1024) end

    result.append("-- ")
    result.append(method.kind.string())
    result.append(" ")

    match method.kind
    | MethodConstructor | MethodFunction =>
      match method.cap
      | let c: String =>
        result.append(c)
        result.append(" ")
      end
    end

    result.append(method.name)
    result.append(
      TypeRenderer.render_type_params(
        method.type_params,
        InfoLinkFormat,
        false,
        include_private))

    result.append("(")
    for (i, param) in method.params.pairs() do
      if i > 0 then result.append(", ") end
      result.append(param.name)
      result.append(": ")
      result.append(
        TypeRenderer.render(
          param.param_type,
          InfoLinkFormat,
          true,
          include_private))
      match param.default_value
      | let dv: String =>
        result.append(" = ")
        result.append(dv)
      end
    end
    result.append(")")

    match method.kind
    | MethodConstructor | MethodFunction =>
      result.append(": ")
      match method.return_type
      | let rt: DocType =>
        result.append(
          TypeRenderer.render(
            rt,
            InfoLinkFormat,
            true,
            include_private))
      end
      if method.is_partial then
        result.append(" ?")
      end
    end
    result.append("\n")

    match method.doc_string
    | let ds: String =>
      result.append("     ")
      result.append(ds)
      result.append("\n")
    end
    result.append("\n")
    consume result

  fun _render_fields(
    fields: Array[DocField] val,
    title: String,
    include_private: Bool)
    : String val
  =>
    if fields.size() == 0 then return "" end

    let result = recover iso String(1024) end
    result.append(title)
    result.append("\n")
    result.append(_underline(title))
    result.append("\n\n")

    for field in fields.values() do
      result.append("-- ")
      result.append(field.kind.string())
      result.append(" ")
      result.append(field.name)
      result.append(": ")
      result.append(
        TypeRenderer.render(
          field.field_type,
          InfoLinkFormat,
          false,
          include_private))
      result.append("\n")

      match field.doc_string
      | let ds: String =>
        result.append("     ")
        result.append(ds)
        result.append("\n")
      end
      result.append("\n")
    end
    consume result

  fun _method_index_entries(
    entity: DocEntity box,
    include_private: Bool)
    : String val
  =>
    let result = recover iso String(1024) end
    for method in entity.constructors.values() do
      result.append("\0\b[index\0\b]\n")
      result.append("* ")
      result.append(entity.name)
      result.append(".")
      result.append(method.name)
      result.append(":  *note ")
      result.append(entity.name)
      result.append(": ")
      result.append(entity.tqfn)
      result.append(".\n")
    end
    for method in entity.public_behaviours.values() do
      result.append("\0\b[index\0\b]\n")
      result.append("* ")
      result.append(entity.name)
      result.append(".")
      result.append(method.name)
      result.append(":  *note ")
      result.append(entity.name)
      result.append(": ")
      result.append(entity.tqfn)
      result.append(".\n")
    end
    for method in entity.public_functions.values() do
      result.append("\0\b[index\0\b]\n")
      result.append("* ")
      result.append(entity.name)
      result.append(".")
      result.append(method.name)
      result.append(":  *note ")
      result.append(entity.name)
      result.append(": ")
      result.append(entity.tqfn)
      result.append(".\n")
    end
    if include_private then
      for method in entity.private_behaviours.values() do
        result.append("\0\b[index\0\b]\n")
        result.append("* ")
        result.append(entity.name)
        result.append(".")
        result.append(method.name)
        result.append(":  *note ")
        result.append(entity.name)
        result.append(": ")
        result.append(entity.tqfn)
        result.append(".\n")
      end
      for method in entity.private_functions.values() do
        result.append("\0\b[index\0\b]\n")
        result.append("* ")
        result.append(entity.name)
        result.append(".")
        result.append(method.name)
        result.append(":  *note ")
        result.append(entity.name)
        result.append(": ")
        result.append(entity.tqfn)
        result.append(".\n")
      end
    end
    consume result

  fun _find_up(
    nodes: Array[(String val, String val)],
    idx: USize,
    program: DocProgram box)
    : String
  =>
    """
    Entity nodes point up to their package; package nodes point up to Top.
    """
    try
      let node_name = nodes(idx)?._1
      // Check if this is a package node
      for mod in program.modules.values() do
        if _pkg_node_name(mod.qualified_name) == node_name then
          return "Top"
        end
        // Check if this is an entity under this module
        for entity in mod.public_types.values() do
          if entity.tqfn == node_name then
            return _pkg_node_name(mod.qualified_name)
          end
        end
        for entity in mod.private_types.values() do
          if entity.tqfn == node_name then
            return _pkg_node_name(mod.qualified_name)
          end
        end
      end
    else
      _Unreachable()
    end
    "Top"

  fun _build_tag_table(
    entries: Array[(String val, USize)])
    : String val
  =>
    let result = recover iso String(2048) end
    result.append("\x1F\nTag Table:\n")
    for entry in entries.values() do
      result.append("Node: ")
      result.append(entry._1)
      result.append("\x7F")
      result.append(entry._2.string())
      result.append("\n")
    end
    result.append("\x1F\nEnd Tag Table\n")
    consume result
