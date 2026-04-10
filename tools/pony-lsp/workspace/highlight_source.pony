use "pony_compiler"

primitive HighlightSource
  """
  Source-text helpers for document highlight classification.

  The ponyc sugar pass strips inline field initializers from field AST nodes
  (replacing child 2 with `tk_none`) before the LSP sees the tree.
  Methods here recover that information by scanning the original source text.
  """

  fun field_has_initializer(ast: AST box): Bool =>
    """
    Return true if the field declaration had an inline `= expr` initializer.

    Pony type syntax never uses `=` in a field's type annotation, so any `=`
    found on the field's source line after the declaration keyword is the
    initializer separator.
    """
    try
      let src = ast.source_contents() as String box
      let l = ast.line()  // 1-indexed
      let line_start: USize =
        if l == 1 then
          0
        else
          (src.find("\n" where nth = l - 2)? + 1).usize()
        end
      let line_end: USize =
        try
          src.find("\n" where offset = line_start.isize())?.usize()
        else
          src.size()
        end
      let col = ast.pos()  // 1-indexed
      // Search for '=' starting from the field keyword's column.
      var i = line_start + (col - 1)
      while i < line_end do
        if src(i)? == '=' then
          return true
        end
        i = i + 1
      end
    end
    false
