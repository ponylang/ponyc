class val IgnorePattern
  """
  A parsed gitignore/ignore pattern with metadata about how it should be
  matched.

  Patterns are immutable after construction. The `pattern` field contains the
  cleaned pattern text with escapes resolved and leading `/` stripped. The
  metadata fields (`negated`, `dir_only`, `anchored`) control matching
  behavior in `IgnoreMatcher`.
  """
  let pattern: String val
  let negated: Bool
  let dir_only: Bool
  let anchored: Bool

  new val create(
    pattern': String val,
    negated': Bool,
    dir_only': Bool,
    anchored': Bool)
  =>
    """
    Create a pattern from pre-parsed components. Callers are responsible for
    resolving escapes and stripping leading `/` before construction; use
    `PatternParser` for full gitignore line parsing.
    """
    pattern = pattern'
    negated = negated'
    dir_only = dir_only'
    anchored = anchored'

primitive PatternParser
  """
  Parses a single line from a `.gitignore` or `.ignore` file into an
  `IgnorePattern`, or returns `None` for blank lines and comments.

  Processing order follows gitignore semantics: detect comments, strip
  trailing whitespace, detect negation, detect directory-only marker, detect
  anchoring, strip leading `/`, and resolve backslash escapes.
  """

  fun apply(line: String val): (IgnorePattern val | None) =>
    """
    Parse a single ignore file line. Returns `None` for blank lines and
    comments.
    """
    // Skip blank lines
    if _is_blank(line) then return None end

    var s: String val = line
    var escaped_leading = false

    // Handle escaped leading # or ! — the backslash prevents the character
    // from being interpreted as a comment or negation marker
    try
      if (s.size() >= 2) and (s(0)? == '\\') then
        let next = s(1)?
        if (next == '#') or (next == '!') then
          s = s.substring(1)
          escaped_leading = true
        end
      end
    end

    // Skip comments (unescaped leading #)
    try
      if (not escaped_leading) and (s(0)? == '#') then return None end
    else
      return None
    end

    // Strip trailing unescaped whitespace
    s = _rstrip(s)
    if s.size() == 0 then return None end

    // Detect and remove negation prefix (only if ! was not escaped)
    var negated = false
    if not escaped_leading then
      try
        if s(0)? == '!' then
          negated = true
          s = s.substring(1)
        end
      end
    end
    if s.size() == 0 then return None end

    // Detect and remove trailing / (directory-only marker)
    var dir_only = false
    try
      if s(s.size() - 1)? == '/' then
        dir_only = true
        s = s.substring(0, (s.size() - 1).isize())
      end
    end
    if s.size() == 0 then return None end

    // Detect anchoring: pattern contains / (after trailing / removal)
    let anchored = s.contains("/")

    // Strip leading /
    try
      if s(0)? == '/' then
        s = s.substring(1)
      end
    end
    if s.size() == 0 then return None end

    // Resolve remaining backslash escapes
    s = _unescape(s)

    IgnorePattern(s, negated, dir_only, anchored)

  fun _is_blank(s: String val): Bool =>
    """
    Return true if the line contains only whitespace.
    """
    for ch in s.values() do
      if (ch != ' ') and (ch != '\t') and (ch != '\r') then
        return false
      end
    end
    true

  fun _rstrip(s: String val): String val =>
    """
    Strip trailing whitespace, preserving escaped spaces (backslash-space).
    """
    var end_pos = s.size()
    while end_pos > 0 do
      try
        let ch = s(end_pos - 1)?
        if (ch == ' ') or (ch == '\t') then
          // Check if this space is escaped
          if (end_pos >= 2) and (s(end_pos - 2)? == '\\') then
            break
          end
          end_pos = end_pos - 1
        else
          break
        end
      else
        break
      end
    end
    if end_pos == s.size() then
      s
    else
      s.substring(0, end_pos.isize())
    end

  fun _unescape(s: String val): String val =>
    """
    Resolve backslash escape sequences. A backslash followed by any character
    produces that character literally.
    """
    if not s.contains("\\") then return s end
    let out = recover iso String(s.size()) end
    var i: USize = 0
    while i < s.size() do
      try
        if (s(i)? == '\\') and ((i + 1) < s.size()) then
          out.push(s(i + 1)?)
          i = i + 2
        else
          out.push(s(i)?)
          i = i + 1
        end
      else
        i = i + 1
      end
    end
    consume out
