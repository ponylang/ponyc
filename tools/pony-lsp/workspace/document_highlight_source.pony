primitive DocumentHighlightSource
  """
  Source-text helpers for document highlight classification.

  The ponyc sugar pass strips inline field initializers from field AST nodes
  (replacing child 2 with `tk_none`) before the LSP sees the tree.
  Methods here recover that information by scanning the original source text.
  """

  fun field_has_initializer(src: String box, start: USize): Bool =>
    """
    Return true if the field declaration starting at byte offset `start`
    had an inline `= expr` initializer.

    Scans forward from the field keyword. Continuation lines (more indented
    than the keyword column) are included in the scan; a line at the same or
    lesser indentation terminates the search. Line comments (`//`) are
    skipped. Returns false if `=` is not found before the declaration ends.

    Known limitation: `//` inside a string literal in the initializer
    (e.g. `var s: String = "a//b"`) would incorrectly stop the scan early.
    This is rare enough in practice to be acceptable.
    """
    // Compute the 1-based column of the field keyword so we can detect
    // continuation lines (more indented) vs new declarations (same or less).
    var line_start: USize = 0
    var k: USize = start
    while k > 0 do
      k = k - 1
      if (try src(k)? else '\n' end) == '\n' then
        line_start = k + 1
        break
      end
    end
    let kw_col = (start - line_start) + 1  // 1-based column of keyword

    var j = start
    while j < src.size() do
      let c = try src(j)? else return false end
      if c == '=' then
        return true
      elseif c == '/' then
        if ((j + 1) < src.size()) and ((try src(j + 1)? else 0 end) == '/') then
          // skip line comment to end of line
          j = j + 2
          while j < src.size() do
            if (try src(j)? else return false end) == '\n' then
              break
            end
            j = j + 1
          end
          // j is at '\n' or end of src; '\n' case handled in next iteration
        else
          j = j + 1
        end
      elseif c == '\n' then
        // count the 1-based column of the first non-whitespace on the next line
        var next_col: USize = 1
        var ni = j + 1
        while ni < src.size() do
          let nc = try src(ni)? else return false end
          if (nc == ' ') or (nc == '\t') then
            next_col = next_col + 1
            ni = ni + 1
          else
            break
          end
        end
        if next_col <= kw_col then
          return false  // same or outer indentation: new declaration
        end
        j = j + 1  // advance past '\n'; scan continues into continuation line
      else
        j = j + 1
      end
    end
    false
