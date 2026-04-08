use "pony_compiler"

primitive ASTIdentifier
  """
  Maps an AST node to the sub-node whose source span should be used as its
  highlight or reference span — usually the identifier token.
  """
  fun identifier_node(ast: AST box): AST box =>
    """
    Return the identifier child node of a compound AST node — the tk_id token
    that carries the symbol's name and source span. Returns `ast` itself if no
    such child is found.
    """
    match ast.id()
    | TokenIds.tk_class()
    | TokenIds.tk_actor()
    | TokenIds.tk_trait()
    | TokenIds.tk_interface()
    | TokenIds.tk_primitive()
    | TokenIds.tk_type()
    | TokenIds.tk_struct()
    | TokenIds.tk_flet()
    | TokenIds.tk_fvar()
    | TokenIds.tk_embed()
    | TokenIds.tk_let()
    | TokenIds.tk_var()
    | TokenIds.tk_param() =>
      // Declarations: identifier at child(0)
      try
        let id = ast(0)?
        if id.id() == TokenIds.tk_id() then
          return id
        end
      end
    | TokenIds.tk_fun()
    | TokenIds.tk_be()
    | TokenIds.tk_new()
    | TokenIds.tk_nominal()
    | TokenIds.tk_typeref() =>
      // Methods/types: identifier at child(1)
      try
        let id = ast(1)?
        if id.id() == TokenIds.tk_id() then
          return id
        end
      end
    | TokenIds.tk_fvarref()
    | TokenIds.tk_fletref()
    | TokenIds.tk_embedref() =>
      // Field references: field name identifier at child(1)
      try
        let id = ast(1)?
        if id.id() == TokenIds.tk_id() then
          return id
        end
      end
    | TokenIds.tk_funref()
    | TokenIds.tk_beref()
    | TokenIds.tk_newref()
    | TokenIds.tk_newberef()
    | TokenIds.tk_funchain()
    | TokenIds.tk_bechain() =>
      // Method call references: method name is the sibling of
      // the receiver child
      try
        let receiver = ast.child() as AST
        let method = receiver.sibling() as AST
        if method.id() == TokenIds.tk_id() then
          return method
        end
      end
    | TokenIds.tk_reference() =>
      // Variable references: identifier at child(0)
      try
        let id = ast(0)?
        if id.id() == TokenIds.tk_id() then
          return id
        end
      end
    end
    ast
