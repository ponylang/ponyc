primitive IndentationSize is TextRule
  """
  Flags lines whose leading-space indentation is not a multiple of 2.

  Scans each line, counting leading space characters. Lines inside
  triple-quoted strings are skipped (using the same toggle-on-odd-count
  approach as `CommentSpacing`), as are blank lines and lines with zero
  indentation. Tab-indented lines are not flagged here — that is
  `style/hard-tabs`'s concern.
  """
  fun id(): String val => "style/indentation-size"
  fun category(): String val => "style"

  fun description(): String val =>
    "indentation should be a multiple of 2 spaces"

  fun default_status(): RuleStatus => RuleOn

  fun check(source: SourceFile val): Array[Diagnostic val] val =>
    recover val
      let result = Array[Diagnostic val]
      var in_docstring = false
      for (idx, line) in source.lines.pairs() do
        let triple_count = _count_triple_quotes(line)
        if in_docstring or (triple_count > 0) then
          if (triple_count % 2) == 1 then
            in_docstring = not in_docstring
          end
        else
          let spaces = _count_leading_spaces(line)
          if (spaces > 0) and ((spaces % 2) != 0) then
            result.push(Diagnostic(id(),
              "indentation should be a multiple of 2 spaces",
              source.rel_path, idx + 1, 1))
          end
        end
      end
      result
    end

  fun _count_leading_spaces(line: String val): USize =>
    """
    Count the number of leading space characters on a line.
    """
    var count: USize = 0
    var i: USize = 0
    while i < line.size() do
      try
        if line(i)? == ' ' then
          count = count + 1
        else
          return count
        end
      else
        return count
      end
      i = i + 1
    end
    count

  fun _count_triple_quotes(line: String val): USize =>
    """
    Count non-overlapping occurrences of `\"\"\"` on a line, scanning
    left to right.
    """
    var count: USize = 0
    var i: USize = 0
    let size = line.size()
    while (i + 2) < size do
      try
        if (line(i)? == '"') and (line(i + 1)? == '"')
          and (line(i + 2)? == '"')
        then
          count = count + 1
          i = i + 3
        else
          i = i + 1
        end
      else
        i = i + 1
      end
    end
    count
