use "pony_compiler"

class val ImplementorsResult
  """
  Types that implement a given trait or interface, split into nominal
  (explicitly declared in the provides list) and structural (matching
  the interface's method set without declaring it).
  """
  let type_name: String val
  let nominal: Array[String val] val
  let structural: Array[String val] val

  new val create(
    type_name': String val,
    nominal': Array[String val] val,
    structural': Array[String val] val)
  =>
    type_name = type_name'
    nominal = nominal'
    structural = structural'

primitive ImplementorsQuery
  """
  Find all types that implement a given trait or interface. Nominal
  implementors are found by scanning provides lists. Structural
  implementors (interfaces only) are found using the compiler's
  subtype checker.
  """

  fun apply(
    session: CompileSession box,
    type_name: String val)
    : (ImplementorsResult val | String val)
  =>
    """
    Find all types that implement the named trait or interface.
    Returns an error string when the type is not found or is not a
    trait or interface.
    """
    let program =
      match \exhaustive\ session.program()
      | let p: Program val => p
      | None => return "no compiled program available"
      end

    let target_def =
      match \exhaustive\ ASTHelpers.find_type_def(program, type_name)
      | let t: AST box => t
      | let err: String val => return err
      | None => return "type not found: " + type_name
      end

    let target_kind = target_def.id()
    if (target_kind != TokenIds.tk_trait()) and
      (target_kind != TokenIds.tk_interface())
    then
      return type_name + " is not a trait or interface"
    end

    let is_interface = target_kind == TokenIds.tk_interface()
    let target_pkg = _entity_package(target_def)

    let nominal_results = Array[String val]
    let structural_results = Array[String val]

    for pkg in program.packages() do
      let pkg_path = pkg.path
      for mod in pkg.modules() do
        var child: (AST box | None) = mod.ast.child()
        while true do
          match \exhaustive\ child
          | let node: AST box =>
            if _is_concrete_entity(node.id()) then
              try
                let name = node(0)?.token_value() as String val

                // Skip private types from other packages
                if name(0)? == '_' then
                  if pkg_path != target_pkg then
                    child = node.sibling()
                    continue
                  end
                end

                if _provides_nominally(node, type_name) then
                  nominal_results.push(name)
                elseif is_interface then
                  let cap = ASTHelpers.default_cap(node)
                  if session.is_subtype(node, cap, target_def, cap) then
                    structural_results.push(name)
                  end
                end
              end
            end
            child = node.sibling()
          | None => break
          end
        end
      end
    end

    _sort(nominal_results)
    _sort(structural_results)

    let nom_out = recover iso Array[String val](nominal_results.size()) end
    for n in nominal_results.values() do nom_out.push(n) end
    let struct_out =
      recover iso Array[String val](structural_results.size()) end
    for s in structural_results.values() do struct_out.push(s) end

    ImplementorsResult(type_name, consume nom_out, consume struct_out)

  fun _provides_nominally(entity: AST box, target: String val): Bool =>
    """
    True when the entity's provides list names `target` directly.
    Walks all children of the provides node and recurses into
    intersection and union types.
    """
    try
      let provides = entity(3)?
      if provides.id() == TokenIds.tk_provides() then
        var child: (AST box | None) = provides.child()
        while true do
          match \exhaustive\ child
          | let c: AST box =>
            if _provides_walk(c, target) then return true end
            child = c.sibling()
          | None => break
          end
        end
      end
    end
    false

  fun _provides_walk(node: AST box, target: String val): Bool =>
    match node.id()
    | TokenIds.tk_nominal() =>
      try
        let name = node(1)?.token_value() as String val
        if name == target then return true end
      end
      false
    | TokenIds.tk_isecttype() | TokenIds.tk_uniontype() =>
      var child: (AST box | None) = node.child()
      while true do
        match \exhaustive\ child
        | let c: AST box =>
          if _provides_walk(c, target) then return true end
          child = c.sibling()
        | None => break
        end
      end
      false
    else
      false
    end

  fun _is_concrete_entity(token_id: TokenId): Bool =>
    match token_id
    | TokenIds.tk_class()
    | TokenIds.tk_actor()
    | TokenIds.tk_primitive()
    | TokenIds.tk_struct() =>
      true
    else false
    end

  fun _entity_package(entity: AST box): String val =>
    var current: (AST box | None) = entity.parent()
    while true do
      match \exhaustive\ current
      | let ast: AST box =>
        match ast.id()
        | TokenIds.tk_package() =>
          match ast.token_value()
          | let s: String val => return s
          end
          return ""
        end
        current = ast.parent()
      | None => return ""
      end
    end
    ""

  fun _sort(arr: Array[String val] ref) =>
    """
    Simple insertion sort for small result sets.
    """
    var i: USize = 1
    while i < arr.size() do
      try
        let key = arr(i)?
        var j = i
        while (j > 0) and (arr(j - 1)? > key) do
          arr(j)? = arr(j - 1)?
          j = j - 1
        end
        arr(j)? = key
      end
      i = i + 1
    end
