use "pony_compiler"

class val TypeAPIResult
  """
  A type's callable API at a given capability: which methods can be
  called, which cannot, available constructors, and implemented traits.
  """
  let type_name: String val
  let type_kind: String val
  let callable: Array[MethodInfo val] val
  let not_callable: Array[(MethodInfo val, String val)] val
  let constructors: Array[MethodInfo val] val
  let traits: Array[String val] val

  new val create(
    type_name': String val,
    type_kind': String val,
    callable': Array[MethodInfo val] val,
    not_callable': Array[(MethodInfo val, String val)] val,
    constructors': Array[MethodInfo val] val,
    traits': Array[String val] val)
  =>
    type_name = type_name'
    type_kind = type_kind'
    callable = callable'
    not_callable = not_callable'
    constructors = constructors'
    traits = traits'

primitive TypeAPIQuery
  """
  Return the callable methods, uncallable methods, constructors, and
  traits for a type at a given capability. Accepts either a type name
  with explicit capability, or a file position from which the type and
  capability are read directly from the compiler's type AST.
  """

  fun from_type(
    session: CompileSession box,
    type_name: String val,
    cap: String val)
    : (TypeAPIResult val | String val)
  =>
    """
    Look up a type by name and return its API filtered by `cap`.
    For generic types like `Array[String]`, matches the base name.
    """
    if not _is_valid_cap(cap) then
      return "invalid capability: " + cap
    end

    let program =
      match \exhaustive\ session.program()
      | let p: Program val => p
      | None => return "no compiled program available"
      end

    let base_name = _base_type_name(type_name)
    if base_name.size() == 0 then
      return "cannot determine type name from: " + type_name
    end

    let type_ast =
      match \exhaustive\ ASTHelpers.find_type_def(program, base_name)
      | let t: AST box => t
      | let err: String val => return err
      | None => return "type not found: " + type_name
      end

    _build_result(type_ast, type_name, base_name, cap)

  fun from_position(
    session: CompileSession box,
    file: String val,
    line: USize,
    column: USize)
    : (TypeAPIResult val | String val)
  =>
    """
    Find the AST node at the given position, read its type and
    capability from the compiler's type AST, then return that type's
    API.
    """
    let program =
      match \exhaustive\ session.program()
      | let p: Program val => p
      | None => return "no compiled program available"
      end

    let module =
      match \exhaustive\ ASTHelpers.find_module(program, file)
      | let m: Module val => m
      | None => return "file not found in compiled program: " + file
      end

    let index = module.create_position_index()

    let node =
      match \exhaustive\ index.find_node_at(line, column)
      | let n: AST box => n
      | None => return "no AST node at position " +
          line.string() + ":" + column.string()
      end

    let type_ast =
      match \exhaustive\ _get_type_ast(node)
      | let t: AST box => t
      | None =>
        return "no type information at position " +
          line.string() + ":" + column.string()
      end

    _result_from_type_ast(type_ast)

  fun _get_type_ast(node: AST box): (AST box | None) =>
    """
    Get the type AST for a node. Handles the same special cases as
    `Types.get_ast_type` but returns the structured AST instead of a
    string.
    """
    match node.id()
    | TokenIds.tk_letref()
    | TokenIds.tk_varref() =>
      let defs = node.definitions()
      try defs(0)?.ast_type() end
    | TokenIds.tk_nominal()
    | TokenIds.tk_uniontype()
    | TokenIds.tk_isecttype() =>
      node
    else
      node.ast_type()
    end

  fun _result_from_type_ast(type_ast: AST box)
    : (TypeAPIResult val | String val)
  =>
    match type_ast.id()
    | TokenIds.tk_nominal() =>
      _handle_nominal_type(type_ast)
    | TokenIds.tk_uniontype() =>
      _handle_union_type(type_ast)
    | TokenIds.tk_arrow() =>
      try
        let rhs = type_ast(1)?
        _result_from_type_ast(rhs)
      else
        "cannot unwrap viewpoint type"
      end
    else
      "unsupported type kind: " + TokenIds.string(type_ast.id())
    end

  fun _handle_nominal_type(nominal: AST box)
    : (TypeAPIResult val | String val)
  =>
    (let def, let base_name, let cap) =
      try _nominal_def_and_cap(nominal)?
      else return "cannot resolve nominal type"
      end

    let display =
      match nominal.type_string()
      | let s: String val => s
      else base_name + " " + cap
      end

    _build_result(def, display, base_name, cap)

  fun _handle_union_type(union_ast: AST box)
    : (TypeAPIResult val | String val)
  =>
    let type_str =
      match union_ast.type_string()
      | let s: String val => "(" + s + ")"
      else "unknown union"
      end

    let branches = Array[(String val, String val, AST box)]
    for branch in union_ast.children() do
      match branch.id()
      | TokenIds.tk_nominal() =>
        try
          (let def, let name, let cap) =
            _nominal_def_and_cap(branch)?
          branches.push((name, cap, def))
        end
      | TokenIds.tk_arrow() =>
        try
          let rhs = branch(1)?
          if rhs.id() == TokenIds.tk_nominal() then
            (let def, let name, let cap) =
              _nominal_def_and_cap(rhs)?
            branches.push((name, cap, def))
          end
        end
      end
    end

    if branches.size() == 0 then
      return "cannot resolve union branches: " + type_str
    end

    if branches.size() == 1 then
      try
        (let name, let cap, let def) = branches(0)?
        return _build_result(def, type_str, name, cap)
      end
    end

    let branch_callables = Array[Array[MethodInfo val] val]
    for (branch_name, branch_cap, branch_ast) in branches.values() do
      let all = MethodEnumerator.methods(branch_ast, branch_name)
      (let callable, _) =
        MethodEnumerator.callable_at(all, branch_cap)
      branch_callables.push(callable)
    end

    let intersected_names = Array[String val]
    try
      for m in branch_callables(0)?.values() do
        if _in_all_branches(m.name, branch_callables) then
          intersected_names.push(m.name)
        end
      end
    end

    let callable_out: Array[MethodInfo val] iso =
      recover iso
        Array[MethodInfo val](intersected_names.size())
      end
    for name in intersected_names.values() do
      try
        let first_m =
          _find_method_by_name(branch_callables(0)?, name)?
        if _signatures_match(name, branch_callables) then
          callable_out.push(
            MethodInfo(
              first_m.name,
              first_m.receiver_cap,
              first_m.params,
              first_m.return_type,
              first_m.is_partial,
              first_m.kind,
              type_str))
        else
          callable_out.push(
            MethodInfo(
              first_m.name,
              "",
              recover val Array[ParamInfo val] end,
              "",
              false,
              first_m.kind,
              type_str))
        end
      end
    end

    TypeAPIResult(
      type_str,
      "union",
      consume callable_out,
      recover val Array[(MethodInfo val, String val)] end,
      recover val Array[MethodInfo val] end,
      recover val Array[String val] end)

  fun _nominal_def_and_cap(nominal: AST box)
    : (AST box, String val, String val) ?
  =>
    """
    Extract the definition, base name, and capability from a
    TK_NOMINAL type AST node. The definition comes from the
    compiler's data pointer (set during the names pass); the
    capability comes from child 3.
    """
    let defs = nominal.definitions()
    let def = defs(0)?

    let base_name =
      match nominal(1)?.token_value()
      | let s: String val => s
      else error
      end

    let cap_node = nominal(3)?
    let cap = CapSubtyping.cap_string(cap_node.id())
    if cap.size() == 0 then error end

    (def, base_name, cap)

  fun _build_result(
    type_ast: AST box,
    display_name: String val,
    base_name: String val,
    cap: String val)
    : TypeAPIResult val
  =>
    let all = MethodEnumerator.methods(type_ast, base_name)
    (let callable, let not_callable) =
      MethodEnumerator.callable_at(all, cap)
    let constructors = _extract_constructors(all)
    let traits = _extract_traits(type_ast)
    let kind = _type_kind(type_ast.id())

    TypeAPIResult(
      display_name,
      kind,
      callable,
      not_callable,
      constructors,
      traits)

  fun _extract_constructors(all: Array[MethodInfo val] val)
    : Array[MethodInfo val] val
  =>
    recover val
      let result = Array[MethodInfo val]
      for m in all.values() do
        if m.kind == "new" then result.push(m) end
      end
      result
    end

  fun _extract_traits(type_ast: AST box): Array[String val] val =>
    let result = Array[String val]
    try
      let provides = type_ast(3)?
      if provides.id() == TokenIds.tk_provides() then
        var child: (AST box | None) = provides.child()
        while true do
          match \exhaustive\ child
          | let c: AST box =>
            _collect_trait_names(c, result)
            child = c.sibling()
          | None => break
          end
        end
      end
    end
    let out = recover iso Array[String val](result.size()) end
    for s in result.values() do out.push(s) end
    consume out

  fun _collect_trait_names(
    node: (AST box | None),
    result: Array[String val] ref)
  =>
    match node
    | let ast: AST box =>
      match ast.id()
      | TokenIds.tk_nominal() =>
        match ast.type_string()
        | let s: String val =>
          result.push(s)
        else
          try
            let defs = ast.definitions()
            let def = defs(0)?
            match def(0)?.token_value()
            | let s: String val => result.push(s)
            end
          end
        end
      | TokenIds.tk_isecttype() | TokenIds.tk_uniontype() =>
        var child: (AST box | None) = ast.child()
        while true do
          match \exhaustive\ child
          | let c: AST box =>
            _collect_trait_names(c, result)
            child = c.sibling()
          | None => break
          end
        end
      end
    end

  fun _base_type_name(type_str: String val): String val =>
    var s = type_str

    // Strip all viewpoint prefixes ("this->", "A->")
    try
      let arrow = s.rfind("->")?
      s = s.trim((arrow + 2).usize())
    end

    // Union types are handled separately
    if try s(0)? == '(' else false end then
      return ""
    end

    // Base name ends at the first '[' or ' '
    var i: USize = 0
    while i < s.size() do
      try
        let c = s(i)?
        if (c == '[') or (c == ' ') then break end
      end
      i = i + 1
    end
    s.trim(0, i)

  fun _has_method_name(
    methods: Array[MethodInfo val] val,
    name: String val)
    : Bool
  =>
    for m in methods.values() do
      if m.name == name then return true end
    end
    false

  fun _find_method_by_name(
    methods: Array[MethodInfo val] val,
    name: String val)
    : MethodInfo val ?
  =>
    for m in methods.values() do
      if m.name == name then return m end
    end
    error

  fun _in_all_branches(
    name: String val,
    branch_callables: Array[Array[MethodInfo val] val])
    : Bool
  =>
    var i: USize = 1
    while i < branch_callables.size() do
      try
        if not _has_method_name(branch_callables(i)?, name) then
          return false
        end
      else
        return false
      end
      i = i + 1
    end
    true

  fun _signatures_match(
    name: String val,
    branch_callables: Array[Array[MethodInfo val] val])
    : Bool
  =>
    if branch_callables.size() <= 1 then return true end
    try
      let first =
        _find_method_by_name(branch_callables(0)?, name)?
      var i: USize = 1
      while i < branch_callables.size() do
        let other =
          _find_method_by_name(branch_callables(i)?, name)?
        if first.receiver_cap != other.receiver_cap then
          return false
        end
        if first.return_type != other.return_type then
          return false
        end
        if first.is_partial != other.is_partial then
          return false
        end
        if first.params.size() != other.params.size() then
          return false
        end
        var j: USize = 0
        while j < first.params.size() do
          if first.params(j)?.param_type !=
            other.params(j)?.param_type
          then
            return false
          end
          j = j + 1
        end
        i = i + 1
      end
      true
    else
      false
    end

  fun _is_valid_cap(cap: String val): Bool =>
    match cap
    | "iso" | "trn" | "ref" | "val" | "box" | "tag" => true
    else false
    end

  fun _type_kind(token_id: TokenId): String val =>
    match token_id
    | TokenIds.tk_class() => "class"
    | TokenIds.tk_actor() => "actor"
    | TokenIds.tk_primitive() => "primitive"
    | TokenIds.tk_trait() => "trait"
    | TokenIds.tk_interface() => "interface"
    | TokenIds.tk_struct() => "struct"
    else ""
    end
