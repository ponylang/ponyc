use "pony_compiler"

class val ParamInfo
  """
  One parameter of a method: name, type, and whether it has a default
  value.
  """
  let name: String val
  let param_type: String val
  let has_default: Bool

  new val create(
    name': String val,
    param_type': String val,
    has_default': Bool = false)
  =>
    name = name'
    param_type = param_type'
    has_default = has_default'

class val MethodInfo
  """
  A method's signature: name, receiver capability, parameters, return
  type, partiality, kind (fun/be/new), and the type it was defined on.
  """
  let name: String val
  let receiver_cap: String val
  let params: Array[ParamInfo val] val
  let return_type: String val
  let is_partial: Bool
  let kind: String val
  let source_type: String val

  new val create(
    name': String val,
    receiver_cap': String val,
    params': Array[ParamInfo val] val,
    return_type': String val,
    is_partial': Bool,
    kind': String val,
    source_type': String val)
  =>
    name = name'
    receiver_cap = receiver_cap'
    params = params'
    return_type = return_type'
    is_partial = is_partial'
    kind = kind'
    source_type = source_type'

primitive MethodEnumerator
  """
  Collect methods from a type's AST, including inherited methods from
  the provides list. A method on the type itself shadows an inherited
  method with the same name.
  """

  fun methods(type_ast: AST box, type_name: String val)
    : Array[MethodInfo val] val
  =>
    """
    All methods on `type_ast`, including those inherited through the
    provides list.
    """
    let result = Array[MethodInfo val]
    let own = _own_methods(type_ast, type_name)
    let own_names = _collect_names(own)
    let inherited = _inherited_methods(type_ast, own_names)
    for m in own.values() do result.push(m) end
    for m in inherited.values() do result.push(m) end
    let out: Array[MethodInfo val] iso =
      recover iso
        let arr = Array[MethodInfo val](result.size())
        arr
      end
    for m in result.values() do out.push(m) end
    consume out

  fun callable_at(
    all_methods: Array[MethodInfo val] val,
    cap: String val)
    : (Array[MethodInfo val] val, Array[(MethodInfo val, String val)] val)
  =>
    """
    Split methods into callable and not-callable for a receiver with
    the given capability. Constructors are excluded — they are always
    callable regardless of receiver cap.
    """
    let callable = recover iso Array[MethodInfo val] end
    let not_callable = recover iso Array[(MethodInfo val, String val)] end

    for m in all_methods.values() do
      if m.kind == "new" then continue end

      if m.kind == "be" then
        // Behaviors are callable on tag and anything that subtypes tag
        callable.push(m)
      elseif CapSubtyping.is_sub(cap, m.receiver_cap) then
        callable.push(m)
      else
        not_callable.push(
          (m, cap + " is not a subtype of " + m.receiver_cap))
      end
    end

    (consume callable, consume not_callable)

  fun _own_methods(type_ast: AST box, type_name: String val)
    : Array[MethodInfo val]
  =>
    let result = Array[MethodInfo val]
    try
      let members = type_ast(4)?
      if members.id() != TokenIds.tk_members() then return result end

      var child: (AST box | None) = members.child()
      while true do
        match \exhaustive\ child
        | let node: AST box =>
          match node.id()
          | TokenIds.tk_fun() | TokenIds.tk_be() | TokenIds.tk_new() =>
            match _extract_method(node, type_name)
            | let m: MethodInfo val => result.push(m)
            end
          end
          child = node.sibling()
        | None => break
        end
      end
    end
    result

  fun _extract_method(method_ast: AST box, type_name: String val)
    : (MethodInfo val | None)
  =>
    try
      let kind: String val =
        match method_ast.id()
        | TokenIds.tk_fun() => "fun"
        | TokenIds.tk_be() => "be"
        | TokenIds.tk_new() => "new"
        else return None
        end

      let cap_node = method_ast(0)?
      let receiver_cap = CapSubtyping.cap_string(cap_node.id())

      let name_node = method_ast(1)?
      let name =
        match name_node.token_value()
        | let s: String val => s
        else return None
        end

      let params_node = method_ast(3)?
      let params = _extract_params(params_node)

      let return_type: String val =
        if kind == "be" then
          "None val"
        else
          try
            let rt = method_ast(4)?
            match rt.type_string()
            | let s: String val => s
            else ""
            end
          else
            ""
          end
        end

      let is_partial =
        try
          method_ast(6)?.id() == TokenIds.tk_question()
        else
          false
        end

      MethodInfo(
        name,
        receiver_cap,
        params,
        return_type,
        is_partial,
        kind,
        type_name)
    end

  fun _extract_params(params_node: AST box): Array[ParamInfo val] val =>
    if params_node.id() != TokenIds.tk_params() then
      return recover val Array[ParamInfo val] end
    end

    recover val
      let result = Array[ParamInfo val]
      var child: (AST box | None) = params_node.child()
      while true do
        match \exhaustive\ child
        | let param: AST box =>
          if param.id() == TokenIds.tk_param() then
            try
              let name_node = param(0)?
              let name =
                match name_node.token_value()
                | let s: String val => s
                else ""
                end
              let param_type =
                try
                  let type_node = param(1)?
                  if type_node.id() != TokenIds.tk_none() then
                    match type_node.type_string()
                    | let s: String val => s
                    else ""
                    end
                  else ""
                  end
                else ""
                end
              let has_default =
                try
                  param(2)?.id() != TokenIds.tk_none()
                else
                  false
                end
              result.push(ParamInfo(name, param_type, has_default))
            end
          end
          child = param.sibling()
        | None => break
        end
      end
      result
    end

  fun _inherited_methods(
    type_ast: AST box,
    own_names: Array[String val])
    : Array[MethodInfo val]
  =>
    let result = Array[MethodInfo val]
    try
      let provides = type_ast(3)?
      if provides.id() != TokenIds.tk_provides() then return result end

      let provides_type = provides.child()
      match provides_type
      | let pt: AST box =>
        _collect_from_provides(pt, own_names, result)
      end
    end
    result

  fun _collect_from_provides(
    type_node: AST box,
    exclude_names: Array[String val],
    result: Array[MethodInfo val])
  =>
    match type_node.id()
    | TokenIds.tk_nominal() =>
      let defs = type_node.definitions()
      if defs.size() > 0 then
        try
          let def = defs(0)?
          let type_name =
            try
              match def(0)?.token_value()
              | let s: String val => s
              else ""
              end
            else ""
            end
          if type_name.size() > 0 then
            let inherited = _own_methods(def, type_name)
            for m in inherited.values() do
              if not _name_in(m.name, exclude_names) then
                result.push(m)
              end
            end
            _inherited_methods_recurse(def, exclude_names, result)
          end
        end
      end
    | TokenIds.tk_isecttype() | TokenIds.tk_uniontype() =>
      var child: (AST box | None) = type_node.child()
      while true do
        match \exhaustive\ child
        | let c: AST box =>
          _collect_from_provides(c, exclude_names, result)
          child = c.sibling()
        | None => break
        end
      end
    end

  fun _inherited_methods_recurse(
    type_ast: AST box,
    exclude_names: Array[String val],
    result: Array[MethodInfo val])
  =>
    try
      let provides = type_ast(3)?
      if provides.id() != TokenIds.tk_provides() then return end

      let provides_type = provides.child()
      match provides_type
      | let pt: AST box =>
        let all_names = _collect_result_names(exclude_names, result)
        _collect_from_provides(pt, all_names, result)
      end
    end

  fun _collect_names(infos: Array[MethodInfo val]): Array[String val] =>
    let names = Array[String val](infos.size())
    for m in infos.values() do names.push(m.name) end
    names

  fun _collect_result_names(
    exclude: Array[String val],
    result: Array[MethodInfo val])
    : Array[String val]
  =>
    let names = Array[String val](exclude.size() + result.size())
    for n in exclude.values() do names.push(n) end
    for m in result.values() do names.push(m.name) end
    names

  fun _name_in(name: String val, names: Array[String val]): Bool =>
    for n in names.values() do
      if n == name then return true end
    end
    false
