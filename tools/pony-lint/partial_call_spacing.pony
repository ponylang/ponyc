use ast = "pony_compiler"

primitive PartialCallSpacing is ASTRule
  """
  Flags partial call sites where `?` does not immediately follow `)`.

  At call sites, the partial marker `?` should be attached directly to the
  closing parenthesis with no intervening space: `foo()?` not `foo() ?`.
  This is the opposite convention from method declarations.
  """
  fun id(): String val => "style/partial-call-spacing"
  fun category(): String val => "style"

  fun description(): String val =>
    "'?' at call site must immediately follow ')'"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [ast.TokenIds.tk_call()]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check that the `?` partial marker (child 3) immediately follows `)` with
    no space.
    """
    try
      let partial_node = node(3)?
      if partial_node.id() != ast.TokenIds.tk_question() then
        return recover val Array[Diagnostic val] end
      end
      let q_line = partial_node.line()
      let q_col = partial_node.pos()
      // Check character before ?
      try
        let line_text = source.lines(q_line - 1)?
        if q_col > 1 then
          try
            if line_text((q_col - 2).usize())? == ' ' then
              return recover val
                [Diagnostic(id(),
                  "'?' at call site should immediately follow ')'",
                  source.rel_path, q_line, q_col)]
              end
            end
          end
        end
      end
    end
    recover val Array[Diagnostic val] end
