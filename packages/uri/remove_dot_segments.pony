primitive RemoveDotSegments
  """
  Remove dot segments from a URI path per RFC 3986 section 5.2.4.

  Dot segments (`.` and `..`) are special path segments used for relative
  navigation. This algorithm resolves them into an equivalent path without
  dot segments — e.g., `/a/b/../c` becomes `/a/c`.

  Used internally by `ResolveURI` during reference resolution, but also
  useful on its own for path normalization.
  """
  fun apply(path: String val): String val =>
    """
    Remove all `.` and `..` segments from `path`, returning the normalized
    result. Implements the iterative algorithm from RFC 3986 section 5.2.4.
    """
    // Fast path: if the path can't contain dot segments, return unchanged.
    // Check for substrings "./" and "/." which cover all mid-path and
    // trailing dot segments, plus exact matches for bare "." and "..".
    if (not path.contains("./")) and (not path.contains("/.")) and
      (path != ".") and (path != "..")
    then
      return path
    end

    let input: String ref = path.clone()
    let output = String(path.size())

    while input.size() > 0 do
      if _starts_with(input, "../") then
        // A: strip leading "../"
        input.delete(0, 3)
      elseif _starts_with(input, "./") then
        // A: strip leading "./"
        input.delete(0, 2)
      elseif _starts_with(input, "/./") then
        // B: replace leading "/./" with "/"
        input.delete(0, 2)
      elseif input == "/." then
        // B: replace "/." with "/"
        input.clear()
        input.append("/")
      elseif _starts_with(input, "/../") then
        // C: replace leading "/../" with "/" and pop last output segment
        input.delete(0, 3)
        _remove_last_segment(output)
      elseif input == "/.." then
        // C: replace "/.." with "/" and pop last output segment
        input.clear()
        input.append("/")
        _remove_last_segment(output)
      elseif (input == ".") or (input == "..") then
        // D: remove bare "." or ".."
        input.clear()
      else
        // E: move first path segment from input to output
        _move_segment(input, output)
      end
    end

    output.clone()

  fun _starts_with(s: String box, prefix: String box): Bool =>
    (s.size() >= prefix.size()) and
      (s.compare_sub(prefix, prefix.size()) is Equal)

  fun _remove_last_segment(output: String ref) =>
    """
    Remove the last segment and its preceding "/" (if any) from the output
    buffer. If there is no "/" the entire buffer is cleared.
    """
    try
      var i = output.size()
      while i > 0 do
        i = i - 1
        if output(i)? == '/' then
          output.truncate(i)
          return
        end
      end
    else
      _Unreachable()
    end
    // No "/" found — clear everything
    output.clear()

  fun _move_segment(input: String ref, output: String ref) =>
    """
    Move the first path segment (including initial "/" if any) from the
    input buffer to the output buffer.
    """
    var end_pos: USize = 0
    try
      // Include leading "/" if present
      if (input.size() > 0) and (input(0)? == '/') then
        end_pos = 1
      end
      // Advance to next "/" or end of input
      while end_pos < input.size() do
        if input(end_pos)? == '/' then
          break
        end
        end_pos = end_pos + 1
      end
    else
      _Unreachable()
    end
    output.append(input.substring(0, end_pos.isize()))
    input.delete(0, end_pos)
