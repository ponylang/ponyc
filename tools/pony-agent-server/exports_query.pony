use "pony_compiler"

class val ExportEntry
  """
  One public type in a module.
  """
  let name: String val
  let kind: String val
  let docstring: (String val | None)

  new val create(
    name': String val,
    kind': String val,
    docstring': (String val | None) = None)
  =>
    name = name'
    kind = kind'
    docstring = docstring'

class val ExportsResult
  """
  Every public type in a module, sorted by name.
  """
  let module_path: String val
  let entries: Array[ExportEntry val] val

  new val create(
    module_path': String val,
    entries': Array[ExportEntry val] iso)
  =>
    module_path = module_path'
    entries = consume entries'

primitive ExportsQuery
  """
  List the public types exported by a module.
  """

  fun apply(
    session: CompileSession box,
    module_name: String val)
    : (ExportsResult val | String val)
  =>
    """
    Return every public type in the named module, or a string
    describing why the lookup failed. The module can be specified
    by qualified name (e.g. "builtin") or by absolute path.
    """
    let program =
      match \exhaustive\ session.program()
      | let p: Program val => p
      | None => return "no compiled program available"
      end

    let mod =
      match \exhaustive\ _find_module(program, module_name)
      | let m: Module val => m
      | None => return "module not found: " + module_name
      end

    let entries =
      recover iso
        let arr = Array[ExportEntry val]
        for f in mod.files() do
          _collect_types(f.ast, arr)
        end
        _sort(arr)
        arr
      end

    ExportsResult(mod.path, consume entries)

  fun _find_module(program: Program val, name: String val)
    : (Module val | None)
  =>
    try return program(name)? end
    for mod in program.modules() do
      if (mod.qualified_name == name) or (mod.path == name) then
        return mod
      end
    end

  fun _collect_types(
    module_ast: AST val,
    entries: Array[ExportEntry val] ref)
  =>
    var child_node = module_ast.child()
    while true do
      match \exhaustive\ child_node
      | let ast: AST val =>
        let kind = _type_kind(ast.id())
        if kind.size() > 0 then
          try
            let id_node = ast(0)?
            match id_node.token_value()
            | let n: String val =>
              try
                if n(0)? != '_' then
                  let doc = _docstring(ast)
                  entries.push(ExportEntry(n, kind, doc))
                end
              end
            end
          end
        end
        child_node = ast.sibling()
      | None => break
      end
    end

  fun _type_kind(token_id: TokenId): String val =>
    match token_id
    | TokenIds.tk_class() => "class"
    | TokenIds.tk_actor() => "actor"
    | TokenIds.tk_primitive() => "primitive"
    | TokenIds.tk_trait() => "trait"
    | TokenIds.tk_interface() => "interface"
    | TokenIds.tk_type() => "type_alias"
    | TokenIds.tk_struct() => "struct"
    else ""
    end

  fun _docstring(type_ast: AST box): (String val | None) =>
    try
      let doc_node = type_ast(6)?
      if doc_node.id() == TokenIds.tk_string() then
        doc_node.token_value()
      end
    end

  fun _sort(arr: Array[ExportEntry val] ref) =>
    let size = arr.size()
    if size < 2 then return end
    var i: USize = 1
    while i < size do
      try
        let entry = arr(i)?
        var j = i
        while (j > 0) and (arr(j - 1)?.name > entry.name) do
          arr(j)? = arr(j - 1)?
          j = j - 1
        end
        arr(j)? = entry
      end
      i = i + 1
    end
