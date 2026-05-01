use "pony_compiler"

primitive _ResolveASTTarget
  """
  Validates that `node` is referenceable and resolves it to its canonical
  definition AST node.

  Returns the canonical target `AST val`, or `None` if:
  - the node is a literal (tk_true/false/int/float/string)
  - the node is a synthetic newref inside a same-position tk_call
    (desugared type-literal expressions such as `None`)
  - definition resolution fails
  """
  fun apply(node: AST box): (AST val | None) =>
    let nid = node.id()

    // Literals have no referenceable identity.
    if (nid == TokenIds.tk_true()) or (nid == TokenIds.tk_false())
      or (nid == TokenIds.tk_int()) or (nid == TokenIds.tk_float())
      or (nid == TokenIds.tk_string())
    then
      return None
    end

    // Synthetic tk_newref inside a tk_call at the same position (desugared
    // type-literal expressions such as `None`) have no referenceable identity.
    if nid == TokenIds.tk_newref() then
      try
        let par = node.parent() as AST box
        if (par.id() == TokenIds.tk_call()) and
          (par.position() == node.position())
        then
          return None
        end
      end
    end

    // When the cursor lands on the name identifier of an entity declaration
    // (tk_class, tk_actor, etc.) or type parameter declaration (tk_typeparam),
    // promote to the enclosing node so that references (which resolve to
    // tk_class or tk_typeparam, not their tk_id child) are found by the walker.
    let node' =
      if nid == TokenIds.tk_id() then
        try
          let par = node.parent() as AST box
          match par.id()
          | TokenIds.tk_class()
          | TokenIds.tk_actor()
          | TokenIds.tk_struct()
          | TokenIds.tk_primitive()
          | TokenIds.tk_trait()
          | TokenIds.tk_interface()
          | TokenIds.tk_type()
          | TokenIds.tk_typeparam() =>
            par
          else
            node
          end
        else
          node
        end
      else
        node
      end

    // Resolve the canonical target definition.
    // If the node has no definitions it IS the definition.
    let defs = node'.definitions()
    if defs.size() > 0 then
      try
        AST(defs(0)?.raw)
      end
    else
      AST(node'.raw)
    end
