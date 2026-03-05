primitive LineLength is TextRule
  """
  Flags lines exceeding 80 codepoints.

  Column is reported as 81 (the first character beyond the limit). The count
  uses `String.codepoints()` so multi-byte UTF-8 characters count as one
  codepoint each. Note that compound emoji (e.g., flag sequences) may count
  as multiple codepoints.

  Lines containing a string literal with no spaces that crosses column 80 are
  exempt. Such strings (URLs, file paths, identifiers) cannot be meaningfully
  split, so flagging them produces noise. Strings that contain spaces are not
  exempt because they can be split at space boundaries using compile-time
  string concatenation (`+` on string literals) at zero runtime cost.

  Lines inside triple-quoted strings (docstrings) and lines containing triple-
  quote delimiters are not eligible for exemption — docstring content should be
  wrapped regardless.

  Known limitations:
  - Does not handle escaped backslashes before quotes. A string ending in a
    literal backslash (e.g., "path\\") may cause incorrect string boundary
    detection. This limitation is shared with `CommentSpacing`.
  """
  fun id(): String val => "style/line-length"
  fun category(): String val => "style"
  fun description(): String val => "line exceeds 80 columns"
  fun default_status(): RuleStatus => RuleOn

  fun check(source: SourceFile val): Array[Diagnostic val] val =>
    recover val
      let result = Array[Diagnostic val]
      var in_docstring = false
      for (idx, line) in source.lines.pairs() do
        let triple_count = _count_triple_quotes(line)
        let was_in_docstring = in_docstring
        if (triple_count % 2) == 1 then
          in_docstring = not in_docstring
        end

        let cp_count = line.codepoints()
        if cp_count > 80 then
          let eligible =
            (not was_in_docstring) and (triple_count == 0)
          let exempt =
            if eligible then
              _has_exempt_string(line)
            else
              false
            end
          if not exempt then
            result.push(Diagnostic(
              id(),
              "line exceeds 80 columns (" + cp_count.string() + ")",
              source.rel_path,
              idx + 1,
              81))
          end
        end
      end
      result
    end

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
        _Unreachable()
        i = i + 1
      end
    end
    count

  fun _has_exempt_string(line: String val): Bool =>
    """
    Check whether the line contains a string literal with no spaces that
    crosses column 80 (starts at or before column 80, ends after column 80).

    Scans byte-by-byte, tracking codepoint position (1-indexed) and toggling
    an in-string flag on each unescaped double-quote. Triple-quote sequences
    are defensively skipped (caller already filters triple-quote lines).
    """
    var in_string = false
    var i: USize = 0
    let size = line.size()
    var cp_pos: USize = 0
    var string_start_cp: USize = 0
    var string_has_space: Bool = false

    while i < size do
      try
        let byte = line(i)?

        // Track codepoint position: increment on each codepoint-start byte
        if (byte and 0xC0) != 0x80 then
          cp_pos = cp_pos + 1
        end

        if byte == '"' then
          if (i == 0) or (line(i - 1)? != '\\') then
            if not in_string then
              // Defensive triple-quote skip
              if ((i + 2) < size) and (line(i + 1)? == '"')
                and (line(i + 2)? == '"')
              then
                i = i + 3
                continue
              end
              in_string = true
              string_start_cp = cp_pos
              string_has_space = false
            else
              // String closing
              if (string_start_cp <= 80) and (cp_pos > 80)
                and (not string_has_space)
              then
                return true
              end
              in_string = false
            end
          end
        elseif in_string and (byte == ' ') then
          string_has_space = true
        end
      else
        _Unreachable()
      end
      i = i + 1
    end
    false
