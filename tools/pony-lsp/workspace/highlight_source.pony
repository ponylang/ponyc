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
    found on the field's source line (before a `//` comment) after the
    declaration keyword is the initializer separator. The scan stops at the
    first `//` to avoid treating `var f: U32 // default=0` as having an
    initializer.

    Known limitation: `//` inside a string literal in the initializer
    (e.g. `var s: String = "a//b"`) would incorrectly stop the scan early.
    This is rare enough in practice to be acceptable.
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
      // Search for '=' starting from the field keyword's column,
      // stopping at '//' (line comment start).
      var i = line_start + (col - 1)
      var prev: U8 = 0
      while i < line_end do
        let c = src(i)?
        if (prev == '/') and (c == '/') then
          break  // entered a line comment — no initializer
        elseif c == '=' then
          return true
        end
        prev = c
        i = i + 1
      end
    end
    false
