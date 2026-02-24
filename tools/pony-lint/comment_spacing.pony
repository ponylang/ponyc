primitive CommentSpacing is TextRule
  """
  Flags // comments not followed by exactly one space (or nothing for
  empty // comments).

  Scans left-to-right, tracking double-quote characters to skip string
  literals (toggling an in-string flag on each unescaped double-quote).

  Known limitations:
  - Does not handle triple-quoted strings. A // inside a triple-quoted
    string spanning multiple lines may produce a false positive.
  - Does not handle escaped backslashes before quotes. A string ending
    in a literal backslash (e.g., "path\\") may cause a subsequent //
    comment to be missed.
  - Does not exempt code-only comments (e.g., commented-out code).
  """
  fun id(): String val => "style/comment-spacing"
  fun category(): String val => "style"
  fun description(): String val => "expected one space after //"
  fun default_status(): RuleStatus => RuleOn

  fun check(source: SourceFile val): Array[Diagnostic val] val =>
    recover val
      let result = Array[Diagnostic val]
      for (idx, line) in source.lines.pairs() do
        match _find_comment(line)
        | let col: USize =>
          result.push(Diagnostic(id(), "expected one space after //",
            source.rel_path, idx + 1, col))
        end
      end
      result
    end

  fun _find_comment(line: String val): (USize | None) =>
    """
    Scan line for a // outside string literals. Returns the 1-indexed
    column position if the comment has bad spacing, or None if OK.
    """
    var in_string = false
    var i: USize = 0
    let size = line.size()

    while i < size do
      try
        let byte = line(i)?
        if (byte == '"') and
          ((i == 0) or (line(i - 1)? != '\\'))
        then
          in_string = not in_string
        elseif (not in_string) and (byte == '/') and
          ((i + 1) < size) and (line(i + 1)? == '/')
        then
          // Found a // outside a string
          let col = i + 1  // 1-indexed
          let after = i + 2
          if after >= size then
            // Empty comment `//` at end of line — OK
            return None
          end
          let next = line(after)?
          if next == ' ' then
            // Check it's exactly one space (not multiple)
            if ((after + 1) < size) and (line(after + 1)? == ' ') then
              return col
            end
            return None
          end
          // Not followed by a space
          return col
        end
      end
      i = i + 1
    end
    None
