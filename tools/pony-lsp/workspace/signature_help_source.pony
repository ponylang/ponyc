primitive SignatureHelpSource
  """
  Pure source-text operations for signature help.
  All functions operate on raw String content and integer positions.
  """

  fun scan_to_token(
    src: String box,
    start_line: USize,
    start_col: USize): (USize, USize)
  =>
    """
    Advances through src from (start_line, start_col) (both 1-based),
    skipping whitespace, commas, and line comments (`//`), and returns
    the (line, col) of the first non-skipped character.  Used to jump
    over the space after a trigger character (`,` or `(`) to reach the
    next real token so that `find_node_at` can be called exactly once.
    """
    var byte_offset: USize = 0
    var cur_line: USize = 1
    // Advance byte_offset to the start of start_line.
    while (cur_line < start_line) and (byte_offset < src.size()) do
      try
        if src(byte_offset)? == '\n' then
          cur_line = cur_line + 1
        end
      end
      byte_offset = byte_offset + 1
    end
    // Advance to the cursor column (1-based → skip start_col-1 bytes).
    byte_offset = byte_offset + (start_col - 1)
    // Scan forward, skipping whitespace, commas, and line comments.
    var scan_line: USize = start_line
    var scan_col: USize = start_col
    while byte_offset < src.size() do
      try
        let ch = src(byte_offset)?
        if ch == '\n' then
          scan_line = scan_line + 1
          scan_col = 1
          byte_offset = byte_offset + 1
        elseif (ch == ' ') or (ch == '\t') or (ch == '\r') or (ch == ',') then
          scan_col = scan_col + 1
          byte_offset = byte_offset + 1
        elseif
          (ch == '/') and (try src(byte_offset + 1)? == '/' else false end)
        then
          // Line comment: advance to the '\n' so the outer loop handles it.
          byte_offset = byte_offset + 1
          scan_col = scan_col + 1
          while byte_offset < src.size() do
            try
              if src(byte_offset)? == '\n' then break end
              byte_offset = byte_offset + 1
              scan_col = scan_col + 1
            else
              break
            end
          end
        else
          break
        end
      else
        break
      end
    end
    (scan_line, scan_col)
