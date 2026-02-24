primitive TrailingWhitespace is TextRule
  """
  Flags trailing spaces or tabs at the end of a line.

  Empty lines (zero-length) do not trigger this rule. Column is reported
  as the position of the first trailing whitespace character (1-indexed,
  in bytes — trailing whitespace is always ASCII).
  """
  fun id(): String val => "style/trailing-whitespace"
  fun category(): String val => "style"
  fun description(): String val => "trailing whitespace"
  fun default_status(): RuleStatus => RuleOn

  fun check(source: SourceFile val): Array[Diagnostic val] val =>
    recover val
      let result = Array[Diagnostic val]
      for (idx, line) in source.lines.pairs() do
        if line.size() == 0 then continue end
        // Find the last non-whitespace character
        var last_non_ws: ISize = -1
        var pos: USize = 0
        for byte in line.values() do
          if (byte != ' ') and (byte != '\t') then
            last_non_ws = pos.isize()
          end
          pos = pos + 1
        end
        // If last_non_ws < size-1, there's trailing whitespace
        if last_non_ws < (line.size() - 1).isize() then
          let col = (last_non_ws + 2).usize()
          result.push(Diagnostic(id(), "trailing whitespace",
            source.rel_path, idx + 1, col))
        end
      end
      result
    end
