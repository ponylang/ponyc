primitive LineLength is TextRule
  """
  Flags lines exceeding 80 codepoints.

  Column is reported as 81 (the first character beyond the limit). The count
  uses `String.codepoints()` so multi-byte UTF-8 characters count as one
  codepoint each. Note that compound emoji (e.g., flag sequences) may count
  as multiple codepoints.
  """
  fun id(): String val => "style/line-length"
  fun category(): String val => "style"
  fun description(): String val => "line exceeds 80 columns"
  fun default_status(): RuleStatus => RuleOn

  fun check(source: SourceFile val): Array[Diagnostic val] val =>
    recover val
      let result = Array[Diagnostic val]
      for (idx, line) in source.lines.pairs() do
        let cp_count = line.codepoints()
        if cp_count > 80 then
          result.push(Diagnostic(
            id(),
            "line exceeds 80 columns (" + cp_count.string() + ")",
            source.rel_path,
            idx + 1,
            81))
        end
      end
      result
    end
