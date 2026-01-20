use "debug"

primitive Types
  fun get_ast_type(ast: AST box): (String | None) =>
    """
    Handle some special cases of AST constructs
    for getting a type
    """
    try
      match ast.id()
      // TODO: handle all the cases we fixed below
      | TokenIds.tk_letref() | TokenIds.tk_varref() | TokenIds.tk_match_capture() =>
        let def: Pointer[_AST] val = @ast_data[Pointer[_AST] val](ast.raw)
        if not def.is_null() then
          AST(def).ast_type_string()
        end
      | TokenIds.tk_beref() =>
        // hard-coding to implicit return type of a behavior
        "None val^"
      | TokenIds.tk_nominal() | TokenIds.tk_uniontype() | TokenIds.tk_isecttype() =>
        ast.type_string()
      | TokenIds.tk_fun() | TokenIds.tk_be() | TokenIds.tk_new() =>
        // get the return type
        ast(4)?.type_string()
      | TokenIds.tk_newref() | TokenIds.tk_newberef() | TokenIds.tk_funref() =>
        let funtype = ast.ast_type() as AST
        funtype(3)?.type_string()
      else
        // applies also for these cases
        //| TokenIds.tk_param() | TokenIds.tk_typeref() | TokenIds.tk_call() | TokenIds.tk_fficall() =>
        ast.ast_type_string()
      end
    end

