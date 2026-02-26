use ast = "pony_compiler"

primitive PartialSpacing is ASTRule
  """
  Flags partial method declarations where the `?` lacks surrounding spaces.

  In method declarations, the partial marker `?` should be separated from the
  preceding token by a space (e.g., `fun foo() ? =>` not `fun foo()? =>`).
  The character after `?` should be a space or end-of-line.
  """
  fun id(): String val => "style/partial-spacing"
  fun category(): String val => "style"

  fun description(): String val =>
    "'?' in method declaration needs surrounding spaces"

  fun default_status(): RuleStatus => RuleOn

  fun node_filter(): Array[ast.TokenId] val =>
    [
      ast.TokenIds.tk_fun()
      ast.TokenIds.tk_new()
      ast.TokenIds.tk_be()
    ]

  fun check(node: ast.AST box, source: SourceFile val)
    : Array[Diagnostic val] val
  =>
    """
    Check that the `?` partial marker (child 5) has a space before and a space
    or end-of-line after it.
    """
    try
      let partial_node = node(5)?
      if partial_node.id() != ast.TokenIds.tk_question() then
        return recover val Array[Diagnostic val] end
      end
      let q_line = partial_node.line()
      let q_col = partial_node.pos()
      // Lines in source are 0-indexed, AST lines are 1-based
      try
        let line_text = source.lines(q_line - 1)?
        // Check character before ?
        let before_ok =
          if q_col > 1 then
            try line_text((q_col - 2).usize())? == ' ' else true end
          else
            true
          end
        // Check character after ?
        let after_ok =
          try
            let ch = line_text(q_col.usize())?
            ch == ' '
          else
            // Past end of line — that's fine (? at end of line)
            true
          end
        if (not before_ok) or (not after_ok) then
          return recover val
            [Diagnostic(id(),
              "'?' in method declaration should have surrounding spaces",
              source.rel_path, q_line, q_col)]
          end
        end
      end
    end
    recover val Array[Diagnostic val] end
