primitive HardTabs is TextRule
  """
  Flags tab characters anywhere in a line.

  One diagnostic is produced per tab character. Column is the 1-indexed byte
  position of the tab.
  """
  fun id(): String val => "style/hard-tabs"
  fun category(): String val => "style"
  fun description(): String val => "use spaces for indentation, not tabs"
  fun default_status(): RuleStatus => RuleOn

  fun check(source: SourceFile val): Array[Diagnostic val] val =>
    recover val
      let result = Array[Diagnostic val]
      for (idx, line) in source.lines.pairs() do
        var col: USize = 1
        for byte in line.values() do
          if byte == '\t' then
            result.push(Diagnostic(
              id(),
              "use spaces for indentation, not tabs",
              source.rel_path,
              idx + 1,
              col))
          end
          col = col + 1
        end
      end
      result
    end
