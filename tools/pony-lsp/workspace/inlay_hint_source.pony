primitive InlayHintSource
  """
  Pure source-text operations for inlay hint collection.
  All functions operate on raw String content and integer positions.
  """

  fun is_cap(src: String box, j: USize): Bool ? =>
    """
    Returns true if the word starting at j is exactly a 3-letter concrete
    capability keyword (iso, trn, ref, val, box, tag) with no further
    word characters at j+3. Errors if fewer than 3 bytes are available.
    """
    match (src(j)?, src(j + 1)?, src(j + 2)?)
    | ('i', 's', 'o') | ('t', 'r', 'n') | ('r', 'e', 'f')
    | ('v', 'a', 'l') | ('b', 'o', 'x') | ('t', 'a', 'g')
    =>
      let next =
        try
          src(j + 3)?
        else
          return true
        end
      not ((next >= 'a') and (next <= 'z'))
    else
      false
    end

  fun scan_past_brackets(
    src: String box, start: USize, start_line: USize, start_col: USize)
    : (USize, USize, USize) ?
  =>
    """
    Scans forward past a balanced [...] sequence, tracking line/col.
    Returns (byte_index, line, col) immediately after the closing ']'.
    Errors if the source ends before the brackets close.

    Note: line and block comments are not handled. A ']' inside a
    `//` or `/* */` comment would be miscounted, causing the scan
    to end early. The effect is a silently missing hint, not a wrong
    hint or a crash, because the downstream try block absorbs the error.
    """
    var j: USize = start
    var j_line: USize = start_line
    var j_col: USize = start_col
    var depth: I32 = 0
    while j < src.size() do
      match src(j)?
      | '\n' =>
        j_line = j_line + 1
        j = j + 1
        j_col = 1
      | '[' =>
        depth = depth + 1
        j = j + 1
        j_col = j_col + 1
      | ']' =>
        depth = depth - 1
        j = j + 1
        j_col = j_col + 1
        if depth == 0 then
          return (j, j_line, j_col)
        end
      else
        j = j + 1
        j_col = j_col + 1
      end
    end
    error

  fun has_annotation(src: String box, after_name: USize): Bool =>
    """
    Returns true if a ':' type annotation immediately follows after_name
    (skipping whitespace). Returns false if '=', '\n', or another
    character comes first.
    """
    var j = after_name
    while j < src.size() do
      match try src(j)? else return false end
      | ':' =>
        return true
      | '=' | '\n' =>
        return false
      | ' ' | '\t' =>
        j = j + 1
      else
        return false
      end
    end
    false

  fun has_explicit_cap_after(src: String box, start: USize): Bool =>
    """
    Skips whitespace from start, then returns true if a 3-letter concrete
    capability keyword immediately follows. Returns false otherwise.
    """
    try
      var j = start
      while j < src.size() do
        let c = src(j)?
        if (c == ' ') or (c == '\t') then
          j = j + 1
        elseif (c >= 'a') and (c <= 'z') then
          let w = j
          j = j + 1
          while j < src.size() do
            let wc = src(j)?
            if (wc >= 'a') and (wc <= 'z') then
              j = j + 1
            else
              break
            end
          end
          return ((j - w) == 3) and is_cap(src, w)?
        else
          return false
        end
      end
      false
    else
      false
    end

  fun scan_to_close_paren(
    src: String box, start: USize, start_line: USize, start_col: USize)
    : (USize, USize, USize) ?
  =>
    """
    Scans forward from start to find the outermost closing ')' of a
    function parameter list, tracking bracket and paren depth to skip
    nested generic types and inner parameter groups. String and character
    literals are skipped so a ')' inside a default value (e.g. `= ")"`)
    does not end the scan prematurely.
    Returns (j_after, line, col) where j_after is the byte index immediately
    after ')' and line/col are the 1-based source position immediately
    after ')'.
    Errors if ')' is not found.

    Precondition: start must be positioned before the opening '(' of the
    parameter list. If start is already inside the list, the first ')'
    encountered will decrement paren depth below zero and the closing ')'
    will never be recognised.

    Note: line and block comments are not handled. A ')' inside a
    `//` or `/* */` comment within the parameter list (e.g. a trailing
    comment on a parameter line) would be miscounted, causing the scan
    to end early. The effect is a silently missing hint, not a wrong
    hint or a crash, because the downstream try block absorbs the error.
    """
    var j: USize = start
    var j_line: USize = start_line
    var j_col: USize = start_col
    var brackets: I32 = 0
    var parens: I32 = 0
    while j < src.size() do
      let c = src(j)?
      if c == '\n' then
        j_line = j_line + 1
        j = j + 1
        j_col = 1
      elseif (c == '"') or (c == '\'') then
        // skip string/char literal — any ')' inside must not be counted
        (j, j_line, j_col) = _skip_literal(src, j + 1, j_line, j_col + 1, c)
      elseif c == '[' then
        brackets = brackets + 1
        j = j + 1
        j_col = j_col + 1
      elseif c == ']' then
        brackets = brackets - 1
        j = j + 1
        j_col = j_col + 1
      elseif (c == '(') and (brackets == 0) then
        parens = parens + 1
        j = j + 1
        j_col = j_col + 1
      elseif (c == ')') and (brackets == 0) then
        parens = parens - 1
        if parens == 0 then
          return (j + 1, j_line, j_col + 1)
        end
        j = j + 1
        j_col = j_col + 1
      else
        j = j + 1
        j_col = j_col + 1
      end
    end
    error

  fun _skip_literal(
    src: String box,
    j_start: USize,
    j_line_start: USize,
    j_col_start: USize,
    delim: U8)
    : (USize, USize, USize)
  =>
    """
    Advances past a string or character literal. j_start, j_line_start,
    j_col_start are the position immediately after the opening delimiter.
    Handles \\-escaped characters and newlines. Returns (j, line, col)
    immediately after the closing delimiter, or at end-of-string if the
    literal is unterminated.

    Note: triple-quoted strings are not handled. Each '"' is treated as a
    single-character delimiter, so three consecutive '"' characters are seen
    as an empty string followed by an opening '"'. Triple-quoted strings can
    appear as default parameter values, so a closing '"' inside such a default
    could end the scan early. The effect is a silently missing hint, not a
    wrong hint or a crash.
    """
    var j: USize = j_start
    var j_line: USize = j_line_start
    var j_col: USize = j_col_start
    while j < src.size() do
      try
        let sc = src(j)?
        if sc == '\\' then
          j = j + 2
          j_col = j_col + 2
        elseif sc == delim then
          j = j + 1
          j_col = j_col + 1
          break
        elseif sc == '\n' then
          j_line = j_line + 1
          j = j + 1
          j_col = 1
        else
          j = j + 1
          j_col = j_col + 1
        end
      else
        break
      end
    end
    (j, j_line, j_col)

  fun has_explicit_return_type(src: String box, after_paren: USize): Bool ? =>
    """
    Scans from after_paren to determine the function return type syntax.
    Returns true if ':' is found before '=>' (explicit return type).
    Returns false if '=>' is found first (inferred return type).
    Errors on unexpected input.

    Note: line and block comments are not handled. A `//` or `/* */`
    comment between ')' and '=>' that contains ':' would be mistaken
    for a return type annotation. The effect is a silently missing
    hint, not a wrong hint or a crash.
    """
    var j = after_paren
    while j < src.size() do
      match src(j)?
      | ':' =>
        return true
      | '=' =>
        if ((j + 1) < src.size()) and (src(j + 1)? == '>') then
          return false
        else
          error
        end
      | ' ' | '\t' | '?' | '\n' =>
        j = j + 1
      else
        error
      end
    end
    error

  fun has_explicit_receiver_cap(src: String box, name_start: USize): Bool =>
    """
    Scans backward from name_start to find the word immediately before
    the function name. Returns true if that word is a concrete capability
    keyword (cap is explicit — no hint needed). Returns false if the word
    is 'fun' or another non-cap keyword (cap is implicit — emit hint).
    Returns true (skip hint) on unexpected input.
    """
    try
      var j = name_start
      while j > 0 do
        j = j - 1
        let c = src(j)?
        if (c == ' ') or (c == '\t') then
          None
        elseif c == '\\' then
          // skip backward past a \annotation\ block
          while j > 0 do
            j = j - 1
            if src(j)? == '\\' then
              break
            end
          end
        elseif (c >= 'a') and (c <= 'z') then
          let wend = j + 1
          while j > 0 do
            let wc = src(j - 1)?
            if (wc >= 'a') and (wc <= 'z') then
              j = j - 1
            else
              break
            end
          end
          if (wend - j) == 3 then
            return is_cap(src, j)?
          end
          return false // 'fun' or other non-cap word: cap is implicit
        else
          // Unexpected character: suppress hint. A missing hint is less
          // disruptive than a spurious one, so we err on the side of silence.
          return true
        end
      end
      true // reached start of file without finding keyword; suppress hint
    else
      true // error during scan; suppress hint
    end
