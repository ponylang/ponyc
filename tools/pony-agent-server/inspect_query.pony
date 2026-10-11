use "pony_compiler"

class val InspectResult
  """
  Position-based inspection result: the node's type, its definition
  site, and enclosing context.
  """
  let type_string: String val
  let node_kind: String val
  let line: USize
  let column: USize
  let definition_file: (String val | None)
  let definition_line: USize
  let definition_column: USize
  let definition_kind: (String val | None)
  let declared_type: (String val | None)
  let enclosing_method: (String val | None)
  let enclosing_receiver_cap: (String val | None)
  let enclosing_type: (String val | None)
  let in_recover: Bool
  let sendable: Bool

  new val create(
    type_string': String val,
    node_kind': String val,
    line': USize,
    column': USize,
    definition_file': (String val | None) = None,
    definition_line': USize = 0,
    definition_column': USize = 0,
    definition_kind': (String val | None) = None,
    declared_type': (String val | None) = None,
    enclosing_method': (String val | None) = None,
    enclosing_receiver_cap': (String val | None) = None,
    enclosing_type': (String val | None) = None,
    in_recover': Bool = false,
    sendable': Bool = false)
  =>
    type_string = type_string'
    node_kind = node_kind'
    line = line'
    column = column'
    definition_file = definition_file'
    definition_line = definition_line'
    definition_column = definition_column'
    definition_kind = definition_kind'
    declared_type = declared_type'
    enclosing_method = enclosing_method'
    enclosing_receiver_cap = enclosing_receiver_cap'
    enclosing_type = enclosing_type'
    in_recover = in_recover'
    sendable = sendable'

primitive InspectQuery
  """
  Find the type, definition, and enclosing context of the AST node at
  a given source position.
  """

  fun apply(
    session: CompileSession box,
    file: String val,
    line: USize,
    column: USize)
    : (InspectResult val | String val)
  =>
    """
    Return the inspection result for the node at the given position,
    or a string describing why the lookup failed.
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

    let node_kind = TokenIds.string(node.id())

    let type_str =
      match \exhaustive\ Types.get_ast_type(node)
      | let ts: String val => ts
      | None =>
        match \exhaustive\ _type_from_definition(node)
        | let ts: String val => ts
        | None =>
          return "no type information for " + node_kind +
            " at " + line.string() + ":" + column.string()
        end
      end

    (let def_file, let def_line, let def_col, let def_kind, let decl_type) =
      _definition_info(node)

    (let enc_method, let enc_receiver_cap) =
      match \exhaustive\ ASTHelpers.find_enclosing_method(node)
      | let m: AST box =>
        (try m(1)?.token_value() end, try m(0)?.get_print() end)
      | None => (None, None)
      end

    let enc_type =
      match \exhaustive\ ASTHelpers.find_enclosing_type(node)
      | let t: AST box =>
        try t(0)?.token_value() end
      | None => None
      end

    let in_recover' = ASTHelpers.in_recover_block(node)
    let sendable' =
      match \exhaustive\ node.ast_type()
      | let type_ast: AST box => session.is_sendable(type_ast)
      | None => false
      end

    InspectResult(
      type_str,
      node_kind,
      node.line(),
      node.pos(),
      def_file,
      def_line,
      def_col,
      def_kind,
      decl_type,
      enc_method,
      enc_receiver_cap,
      enc_type,
      in_recover',
      sendable')

  fun _type_from_definition(node: AST box): (String val | None) =>
    let defs = node.definitions()
    if defs.size() > 0 then
      try
        let def = defs(0)?
        match def.id()
        | TokenIds.tk_flet()
        | TokenIds.tk_fvar()
        | TokenIds.tk_embed() =>
          try
            def(1)?.type_string()
          end
        | TokenIds.tk_let()
        | TokenIds.tk_var() =>
          def.ast_type_string()
        | TokenIds.tk_param() =>
          try
            def(1)?.type_string()
          end
        | TokenIds.tk_fun()
        | TokenIds.tk_new() =>
          try
            def(4)?.type_string()
          end
        | TokenIds.tk_be() =>
          "None val^"
        | TokenIds.tk_class()
        | TokenIds.tk_actor()
        | TokenIds.tk_primitive()
        | TokenIds.tk_struct()
        | TokenIds.tk_trait()
        | TokenIds.tk_interface()
        | TokenIds.tk_type() =>
          try
            let id_node = def(0)?
            id_node.token_value()
          end
        end
      end
    end

  fun _definition_info(node: AST box)
    : ((String val | None), USize, USize, (String val | None),
      (String val | None))
  =>
    let defs = node.definitions()
    if defs.size() > 0 then
      try
        let def = defs(0)?
        let def_file =
          match \exhaustive\ def.source_file()
          | let sf: String val => sf
          | None => None
          end
        let def_line = def.line()
        let def_col = def.pos()
        let def_kind = TokenIds.string(def.id())
        let decl_type = _declared_type_at(def)
        return (def_file, def_line, def_col, def_kind, decl_type)
      end
    end
    (None, 0, 0, None, None)

  fun _declared_type_at(def: AST box): (String val | None) =>
    """
    The type as declared at the definition site, not the inferred type
    at the reference.
    """
    match def.id()
    | TokenIds.tk_flet()
    | TokenIds.tk_fvar()
    | TokenIds.tk_embed() =>
      try def(1)?.type_string() end
    | TokenIds.tk_let()
    | TokenIds.tk_var() =>
      def.ast_type_string()
    | TokenIds.tk_param() =>
      try def(1)?.type_string() end
    | TokenIds.tk_fun()
    | TokenIds.tk_new() =>
      try def(4)?.type_string() end
    | TokenIds.tk_be() =>
      "None val^"
    end
