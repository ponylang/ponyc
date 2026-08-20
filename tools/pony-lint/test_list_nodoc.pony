use ast = "pony_compiler"

primitive TestListNodoc is ASTRule
  """
  Flags types that provide `TestList` without a `\nodoc\` annotation.

  `TestList` implementations exist only to register tests with PonyTest.
  They belong in generated documentation no more than the `UnitTest` classes
  they register, so the convention is to annotate them with `\nodoc\`.
  """
  fun id(): String val => "style/testlist-nodoc"
  fun category(): String val => "style"

  fun description(): String val =>
    "TestList provider should have \\nodoc\\ annotation"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [
      ast.TokenIds.tk_class(); ast.TokenIds.tk_actor()
      ast.TokenIds.tk_primitive(); ast.TokenIds.tk_struct()
    ]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    if node.has_annotation("nodoc") then
      return recover val Array[Diagnostic val] end
    end

    try
      let provides = node(3)?
      if provides.id() == ast.TokenIds.tk_none() then
        return recover val Array[Diagnostic val] end
      end
      if _provides_testlist(provides) then
        let name_node = node(0)?
        return recover val
          [ Diagnostic(
            id(),
            "TestList provider should have \\nodoc\\ annotation",
            source.rel_path,
            name_node.line(),
            name_node.pos()) ]
        end
      end
    end
    recover val Array[Diagnostic val] end

  fun _provides_testlist(node: ast.AST box): Bool =>
    """
    Returns true if the provides subtree names `TestList`, including
    through intersection types.
    """
    let tk = node.id()
    if tk == ast.TokenIds.tk_nominal() then
      try
        match node(1)?.token_value()
        | "TestList" => return true
        end
      end
    elseif (tk == ast.TokenIds.tk_provides()) or
      (tk == ast.TokenIds.tk_isecttype())
    then
      var i: USize = 0
      while true do
        try
          if _provides_testlist(node(i)?) then return true end
        else
          break
        end
        i = i + 1
      end
    end
    false
