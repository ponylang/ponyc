use "debug"
use "assert"



primitive DefinitionResolver
  fun _data_ast(ast: AST box): Array[AST] =>
    let ptr = @ast_data[Pointer[_AST] val](ast.raw)
    if ptr.is_null() then
      [as AST:]
    else
      [AST(ptr)]
    end

  fun _find_in_type(ttype: AST, name: String box, result: Array[AST]): Array[AST] ? =>
    match ttype.id()
    | TokenIds.tk_nominal() =>
        let receiver_def = _data_ast(ttype)(0)?
        Debug("searching inside definition: " + receiver_def.debug())
        let found = receiver_def.find_in_scope(name) as AST
        Debug("FOUND: " + found.debug())
        result.push(found)
        result
    | TokenIds.tk_uniontype() | TokenIds.tk_isecttype() =>
      // recurse into type members
      var iter_result = result
      for type_member in ttype.children() do
        iter_result = _find_in_type(type_member, name, iter_result)?
      end
      iter_result
    else
      result
    end


  fun resolve(ast: AST box): Array[AST] =>
    match ast.id()
    // locals
    | TokenIds.tk_varref() | TokenIds.tk_letref() => _data_ast(ast)
    // parameters
    | TokenIds.tk_paramref() => _data_ast(ast)
    // functions, behaviours and constructors
    // these don't have a reference to the definition
    // in their data field - but we can look them up 
    // via their types symbol table
    | TokenIds.tk_funref() | TokenIds.tk_beref()
    | TokenIds.tk_newref() | TokenIds.tk_newberef()
    | TokenIds.tk_funchain() | TokenIds.tk_bechain() =>
      try
        let receiver = ast.child() as AST
        Debug("RECEIVER " + receiver.debug() )
        let method = receiver.sibling() as AST
        let method_name = method.token_value() as String
        let receiver_type = receiver.ast_type() as AST
        Debug("RECEIVER TYPE " + receiver_type.debug())
        _find_in_type(receiver_type, method_name, [])?
      else
        []
      end
    // fields
    | TokenIds.tk_fvarref() | TokenIds.tk_fletref() | TokenIds.tk_embedref() =>
      let data = _data_ast(ast)
      if data.size() > 0 then
        data
      else
        // no AST in data, we gotta do an expensive lookup
        // a type stores a pointer to its definition in its data field
        try
          let lhs = ast.child() as AST
          let lhs_type = lhs.ast_type() as AST
          let rhs = lhs.sibling() as AST
          Assert(rhs.id() == TokenIds.tk_id(), "RHS not an ID, but " +
          rhs.id().string())?
          let rhs_id = rhs.token_value() as String
          _find_in_type(lhs_type, rhs_id, [])?
        else
          Debug("Error resolving field definiton")
          []
        end
      end
    | TokenIds.tk_typeref() => 
      // typerefs have their definition set in their data field
      _data_ast(ast)
    | TokenIds.tk_typeparamref() =>
      // type param refs have their definition set in their data field
      _data_ast(ast)
    | TokenIds.tk_tupleelemref() =>
      // go to the tuple lhs definition
      try
        let lhs = ast.child() as AST
        // TODO: if the definition has an explicit type definition we can
        // further refine the result to point to the tuple elem type definition
        resolve(lhs)
      else
        Debug("Error resolving tuple element reference")
        []
      end
    | TokenIds.tk_packageref() =>
      try
        let package_alias = (ast.child() as AST).token_value() as String
        // search in the program scope for a package with that name
        // get the program
        Debug("Searching for package " + package_alias)
        var program_ast = ast
        while program_ast.id() != TokenIds.tk_program() do
          program_ast = program_ast.parent() as AST
        end
        for package_ast in program_ast.children() do
          let package = package_ast.package()()?
          if package.qualified_name() == package_alias then
            Debug("Found package " + package.qualified_name())
            // return pointer to the first module in the package
            return [package_ast.child() as AST]
          end
        end
        []
      else
        Debug("Error resolving package reference")
        []
      end
    | TokenIds.tk_nominal() =>
      // the definition of the type is set as ast-data in the names pass
      _data_ast(ast)
    else
      Debug("Dunno how to resolve definition for " + ast.debug())
      []
    end
