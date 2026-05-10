use "pony_compiler"

class val EntityInfo
  """
  Information about an entity declaration.
  """
  let keyword: String
  let name: String
  let type_params: String
  let docstring: String

  new val create(
    keyword': String,
    name': String,
    type_params': String = "",
    docstring': String = "")
  =>
    keyword = keyword'
    name = name'
    type_params = type_params'
    docstring = docstring'

class val MethodInfo
  """
  Information about a method declaration.
  """
  let keyword: String
  let name: String
  let receiver_cap: String
  let type_params: String
  let params: String
  let return_type: String
  let docstring: String

  new val create(
    keyword': String,
    name': String,
    receiver_cap': String = "",
    type_params': String = "",
    params': String = "()",
    return_type': String = "",
    docstring': String = "")
  =>
    keyword = keyword'
    name = name'
    receiver_cap = receiver_cap'
    type_params = type_params'
    params = params'
    return_type = return_type'
    docstring = docstring'

class val FieldInfo
  """
  Information about a field declaration.
  """
  let keyword: String
  let name: String
  let field_type: String

  new val create(keyword': String, name': String, field_type': String = "") =>
    keyword = keyword'
    name = name'
    field_type = field_type'

primitive HoverFormatter
  """
  Extracts hover information from AST nodes and formats as markdown.
  """

  // ======= Pure Formatting Functions =======

  fun format_entity(info: EntityInfo): String =>
    """
    Format entity declaration as markdown hover text.
    """
    let declaration = info.keyword + " " + info.name + info.type_params
    let code_block = _wrap_code_block(consume declaration)
    if info.docstring.size() > 0 then
      code_block + "\n\n" + info.docstring
    else
      code_block
    end

  fun format_method(info: MethodInfo): String =>
    """
    Format method information into markdown hover text.
    """
    let cap_str =
      if info.receiver_cap.size() > 0 then
        " " + info.receiver_cap
      else
        ""
      end
    let signature =
      info.keyword + cap_str + " " + info.name +
      info.type_params + info.params +
      info.return_type
    let code_block = _wrap_code_block(consume signature)
    if info.docstring.size() > 0 then
      code_block + "\n\n" + info.docstring
    else
      code_block
    end

  fun format_field(info: FieldInfo): String =>
    """
    Format field declaration as markdown hover text.
    """
    let declaration = info.keyword + " " + info.name + info.field_type
    _wrap_code_block(consume declaration)

  // ======= AST Extraction and Dispatch =======
  fun create_hover(ast: AST box): (String | None) =>
    """
    Main entry point. Returns markdown string or
    None if no hover info available.
    """
    match ast.id()
    // Method types
    | TokenIds.tk_fun() => _format_method(ast)
    | TokenIds.tk_be() => _format_method(ast)
    | TokenIds.tk_new() => _format_method(ast)

    // Field types
    | TokenIds.tk_flet() => _format_field(ast, "let")
    | TokenIds.tk_fvar() => _format_field(ast, "var")
    | TokenIds.tk_embed() => _format_field(ast, "embed")

    // Local variable declarations
    | TokenIds.tk_let() => _format_local_var(ast, "let")
    | TokenIds.tk_var() => _format_local_var(ast, "var")

    // Parameter declarations
    | TokenIds.tk_param() => _format_param(ast)

    // Type references - follow to definition
    | TokenIds.tk_reference() => _format_reference(ast)
    | TokenIds.tk_typeref() => _format_reference(ast)
    | TokenIds.tk_typealiasref() => _format_reference(ast)
    | TokenIds.tk_nominal() => _format_reference(ast)

    // Function/method/constructor calls
    | TokenIds.tk_funref() => _format_reference(ast)
    | TokenIds.tk_beref() => _format_reference(ast)
    | TokenIds.tk_newref() => _format_reference(ast)
    | TokenIds.tk_newberef() => _format_reference(ast)
    | TokenIds.tk_funchain() => _format_reference(ast)
    | TokenIds.tk_bechain() => _format_reference(ast)

    // Field references
    | TokenIds.tk_fletref() => _format_reference(ast)
    | TokenIds.tk_fvarref() => _format_reference(ast)
    | TokenIds.tk_embedref() => _format_reference(ast)

    // Local variable references
    | TokenIds.tk_letref() => _format_reference(ast)
    | TokenIds.tk_varref() => _format_reference(ast)

    // Parameter references
    | TokenIds.tk_paramref() => _format_reference(ast)

    // Identifier
    | TokenIds.tk_id() => _format_id(ast)

    else
      // For other node types, return None
      None
    end

  fun _format_entity(
    ast: AST box,
    keyword: String): (String | None)
  =>
    """
    Format entity declarations.
    """
    match \exhaustive\ extract_entity_info(ast, keyword)
    | let info: EntityInfo => format_entity(info)
    | None => None
    end

  fun extract_entity_info(
    ast: AST box,
    keyword: String): (EntityInfo | None)
  =>
    """
    Extract entity information from AST node.
    """
    try
      let id = ast(0)?
      if id.id() == TokenIds.tk_id() then
        let name = id.token_value() as String

        // Extract type parameters
        let type_params_str =
          match \exhaustive\ _TypeFormatter._find_child_by_type(
            ast, TokenIds.tk_typeparams(), 1)
          | let tp: AST box => _TypeFormatter.extract_type_params(tp)
          | None => ""
          end

        // Extract docstring from child 6
        let docstring: String =
          try
            let doc_node = ast(6)?
            if doc_node.id() == TokenIds.tk_string() then
              doc_node.token_value() as String
            else
              ""
            end
          else
            ""
          end

        EntityInfo(keyword, name, type_params_str, docstring)
      else
        None
      end
    else
      None
    end

  fun _format_method(
    ast: AST box): (String | None)
  =>
    """
    Format method declarations.
    """
    match \exhaustive\ extract_method_info(ast)
    | let info: MethodInfo => format_method(info)
    | None => None
    end

  fun extract_method_info(
    ast: AST box): (MethodInfo | None)
  =>
    """
    Extract method information from AST node.
    """
    let token_id = ast.id()
    let keyword: String val =
      if token_id == TokenIds.tk_fun() then "fun"
      elseif token_id == TokenIds.tk_be() then "be"
      elseif token_id == TokenIds.tk_new() then "new"
      else return None
      end

    try
      // Extract receiver capability from child(0)
      let receiver_cap =
        try
          let cap = ast(0)?
          _TypeFormatter.extract_capability(cap.id())
        else
          ""
        end

      let id = ast(1)?
      if id.id() == TokenIds.tk_id() then
        let name = id.token_value() as String

        // Extract type parameters
        let type_params_str =
          try
            let type_params = ast(2)?
            if type_params.id() == TokenIds.tk_typeparams() then
              _TypeFormatter.extract_type_params(type_params)
            else
              ""
            end
          else
            ""
          end

        // Extract parameters
        let params_str =
          try
            let params = ast(3)?
            _extract_params(params)
          else
            "()"
          end

        // Extract return type (only for fun/be, not new)
        let return_type_str =
          if (token_id == TokenIds.tk_fun()) or
            (token_id == TokenIds.tk_be())
          then
            try
              let return_type = ast(4)?
              ": " + _TypeFormatter.extract_type(return_type)
            else
              ""
            end
          else
            ""
          end

        // Extract docstring from child 7
        let docstring: String =
          try
            let doc_node = ast(7)?
            if doc_node.id() == TokenIds.tk_string() then
              doc_node.token_value() as String
            else
              ""
            end
          else
            ""
          end

        MethodInfo(
          keyword,
          name,
          receiver_cap,
          type_params_str,
          params_str,
          return_type_str,
          docstring)
      else
        None
      end
    else
      None
    end

  fun _format_field(
    ast: AST box,
    keyword: String): (String | None)
  =>
    """
    Format field declarations.
    """
    match \exhaustive\ _extract_field_info(ast, keyword)
    | let info: FieldInfo => format_field(info)
    | None => None
    end

  fun _extract_field_info(
    ast: AST box,
    keyword: String): (FieldInfo | None)
  =>
    """
    Extract field information from AST node.
    """
    try
      let id = ast(0)?
      if id.id() == TokenIds.tk_id() then
        let name = id.token_value() as String

        // Extract type annotation
        let type_str =
          try
            let field_type = ast(1)?
            ": " + _TypeFormatter.extract_type(field_type)
          else
            ""
          end

        FieldInfo(keyword, name, type_str)
      else
        None
      end
    else
      None
    end

  fun _format_local_var(
    ast: AST box,
    keyword: String): (String | None)
  =>
    """
    Format local variable declarations.
    """
    match \exhaustive\ _extract_local_var_info(ast, keyword)
    | let info: FieldInfo => format_field(info)
    | None => None
    end

  fun _extract_local_var_info(
    ast: AST box,
    keyword: String): (FieldInfo | None)
  =>
    """
    Extract local variable information from AST node.
    """
    try
      let id = ast(0)?
      if id.id() == TokenIds.tk_id() then
        let name = id.token_value() as String

        // Extract type annotation if present
        let type_str =
          try
            let var_type = ast(1)?
            if var_type.id() != TokenIds.tk_none() then
              ": " + _TypeFormatter.extract_type(var_type)
            else
              ""
            end
          else
            ""
          end

        FieldInfo(keyword, name, type_str)
      else
        None
      end
    else
      None
    end

  fun _format_param(ast: AST box): (String | None) =>
    """
    Format parameter declarations.
    """
    match \exhaustive\ _extract_param_info(ast)
    | let info: FieldInfo => format_field(info)
    | None => None
    end

  fun _extract_param_info(ast: AST box): (FieldInfo | None) =>
    """
    Extract parameter information from AST node.
    """
    try
      let id = ast(0)?
      if id.id() == TokenIds.tk_id() then
        let name = id.token_value() as String

        // Extract type annotation
        let type_str =
          try
            let param_type = ast(1)?
            if param_type.id() != TokenIds.tk_none() then
              ": " + _TypeFormatter.extract_type(param_type)
            else
              ""
            end
          else
            ""
          end

        FieldInfo("param", name, type_str)
      else
        None
      end
    else
      None
    end

  fun _format_declaration(ast: AST box): (String | None) =>
    """
    Format a declaration AST node based on its type. Returns None for
    non-declaration node types.
    """
    match ast.id()
    | TokenIds.tk_class() => _format_entity(ast, "class")
    | TokenIds.tk_actor() => _format_entity(ast, "actor")
    | TokenIds.tk_trait() => _format_entity(ast, "trait")
    | TokenIds.tk_interface() => _format_entity(ast, "interface")
    | TokenIds.tk_primitive() => _format_entity(ast, "primitive")
    | TokenIds.tk_type() => _format_entity(ast, "type")
    | TokenIds.tk_struct() => _format_entity(ast, "struct")
    | TokenIds.tk_fun() => _format_method(ast)
    | TokenIds.tk_be() => _format_method(ast)
    | TokenIds.tk_new() => _format_method(ast)
    | TokenIds.tk_flet() => _format_field(ast, "let")
    | TokenIds.tk_fvar() => _format_field(ast, "var")
    | TokenIds.tk_embed() => _format_field(ast, "embed")
    | TokenIds.tk_let() => _format_local_var(ast, "let")
    | TokenIds.tk_var() => _format_local_var(ast, "var")
    | TokenIds.tk_param() => _format_param(ast)
    else
      None
    end

  fun _format_id(ast: AST box): (String | None) =>
    """
    Format identifier nodes - look at parent to
    get full context, or follow to definition.
    """
    match ast.parent()
    | let parent: AST =>
      match parent.id()
      | TokenIds.tk_letref()
      | TokenIds.tk_varref()
      | TokenIds.tk_fletref()
      | TokenIds.tk_fvarref()
      | TokenIds.tk_embedref()
      | TokenIds.tk_paramref() =>
        _format_reference(parent)
      else
        match \exhaustive\ _format_declaration(parent)
        | let s: String => s
        | None => _format_from_definition(ast)
        end
      end
    else
      _format_from_definition(ast)
    end

  fun _format_reference(ast: AST box): (String | None) =>
    """
    Format type reference nodes by following to their definition.
    """
    _format_from_definition(ast)

  fun _format_from_definition(ast: AST box): (String | None) =>
    """
    Follow an identifier or reference to its definition and format that.
    """
    try
      let defs = ast.definitions()
      if defs.size() > 0 then
        _format_declaration(defs(0)?)
      else
        None
      end
    else
      None
    end

  fun _extract_params(params: AST box): String =>
    """
    Extract parameter list from params node.
    """
    if params.id() == TokenIds.tk_params() then
      let param_strs = Array[String]
      for param in params.children() do
        try
          let param_id = param(0)?
          if param_id.id() == TokenIds.tk_id() then
            let param_name = param_id.token_value() as String
            let param_type =
              try
                let ptype = param(1)?
                ": " + _TypeFormatter.extract_type(ptype)
              else
                ""
              end
            param_strs.push(param_name + param_type)
          end
        end
      end
      "(" + ", ".join(param_strs.values()) + ")"
    else
      "()"
    end

  fun _wrap_code_block(content: String): String =>
    """
    Wrap content in a Pony code block for markdown formatting.
    """
    "```pony\n" + content + "\n```"
