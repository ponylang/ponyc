use "pony_compiler"

class val ScopeEntry
  """
  One name visible at a source position. When `accessible` is false,
  `reason` explains why the name cannot be used (typically because it
  is blocked by a recover expression).
  """
  let name: String val
  let type_string: String val
  let kind: String val
  let accessible: Bool
  let reason: (String val | None)

  new val create(
    name': String val,
    type_string': String val,
    kind': String val,
    accessible': Bool = true,
    reason': (String val | None) = None)
  =>
    name = name'
    type_string = type_string'
    kind = kind'
    accessible = accessible'
    reason = reason'

class val ScopeResult
  """
  Every name reachable from a source position, annotated with
  recover-block accessibility.
  """
  let entries: Array[ScopeEntry val] val
  let in_recover: Bool

  new val create(
    entries': Array[ScopeEntry val] iso,
    in_recover': Bool)
  =>
    entries = consume entries'
    in_recover = in_recover'

primitive ScopeQuery
  """
  Collect every name visible at a source position and determine
  whether each is accessible given the recover-block context.
  """

  fun apply(
    session: CompileSession box,
    file: String val,
    line: USize,
    column: USize)
    : (ScopeResult val | String val)
  =>
    """
    Return the scope at the given position, or a string describing
    why the lookup failed.
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

    let in_recover = ASTHelpers.in_recover_block(node)
    let recover_node: (AST box | None) =
      if in_recover then
        ASTHelpers.find_enclosing_recover(node)
      else
        None
      end

    let entries = recover iso Array[ScopeEntry val] end
    let seen = Array[String val]

    var current: (AST box | None) = node
    while true do
      match \exhaustive\ current
      | let ast: AST box =>
        if ast.has_scope() then
          match ast.symbol_table()
          | let st: SymbolTable =>
            for (sym_name, def) in st.iter() do
              try if sym_name(0)? == '$' then continue end end
              if seen.contains(sym_name
                where predicate = {(a, b) => a == b })
              then continue end
              seen.push(sym_name)

              let kind = _classify(def)
              let type_str = _type_string_for(def)
              (let accessible, let reason) =
                _accessibility(
                  session, def, kind, in_recover, recover_node)
              entries.push(
                ScopeEntry(
                  sym_name, type_str, kind, accessible, reason))
            end
          end
        end
        current = ast.parent()
      | None => break
      end
    end

    ScopeResult(consume entries, in_recover)

  fun _classify(def: AST box): String val =>
    match def.id()
    | TokenIds.tk_let() | TokenIds.tk_var() => "local"
    | TokenIds.tk_flet() | TokenIds.tk_fvar()
    | TokenIds.tk_embed() => "field"
    | TokenIds.tk_param() => "parameter"
    | TokenIds.tk_class() | TokenIds.tk_actor()
    | TokenIds.tk_primitive() | TokenIds.tk_struct()
    | TokenIds.tk_trait() | TokenIds.tk_interface()
    | TokenIds.tk_type() => "type"
    | TokenIds.tk_fun() | TokenIds.tk_be()
    | TokenIds.tk_new() => "method"
    | TokenIds.tk_typeparam() => "typeparam"
    else
      "other"
    end

  fun _type_string_for(def: AST box): String val =>
    match def.id()
    | TokenIds.tk_let() | TokenIds.tk_var() =>
      match def.ast_type_string()
      | let s: String val => s
      else ""
      end
    | TokenIds.tk_flet() | TokenIds.tk_fvar() | TokenIds.tk_embed() =>
      try
        match def(1)?.type_string()
        | let s: String val => s
        else ""
        end
      else
        ""
      end
    | TokenIds.tk_param() =>
      try
        match def(1)?.type_string()
        | let s: String val => s
        else ""
        end
      else
        ""
      end
    else
      ""
    end

  fun _accessibility(
    session: CompileSession box,
    def: AST box,
    kind: String val,
    in_recover: Bool,
    recover_node: (AST box | None))
    : (Bool, (String val | None))
  =>
    if not in_recover then return (true, None) end

    if kind == "field" then
      return (false, "this is tag in recover block")
    end

    // Only value bindings need the sendability check
    if (kind != "local") and (kind != "parameter") then
      return (true, None)
    end

    match recover_node
    | let rn: AST box =>
      if ASTHelpers.is_inside(def, rn) then
        return (true, None)
      end
    end

    match def.ast_type()
    | let type_ast: AST box =>
      if session.is_sendable(type_ast) then
        (true, None)
      else
        let cap =
          _trailing_cap(
            match def.ast_type_string()
            | let s: String val => s
            else ""
            end)
        if cap.size() > 0 then
          (false, cap + " not accessible in recover block")
        else
          (false, "not accessible in recover block")
        end
      end
    else
      (true, None)
    end

  fun _trailing_cap(type_str: String val): String val =>
    var s = type_str
    while s.size() > 0 do
      try
        let last = s(s.size() - 1)?
        if (last == '^') or (last == '!') then
          s = s.trim(0, s.size() - 1)
        else
          break
        end
      else
        break
      end
    end
    try
      let idx = s.rfind(" ")?
      let cap = s.trim(idx.usize() + 1)
      try
        let first = cap(0)?
        if (first >= 'a') and (first <= 'z') then cap
        else ""
        end
      else ""
      end
    else
      ""
    end
