primitive GlobMatch
  """
  Glob pattern matching for gitignore-style patterns.

  Supports `*` (matches any characters except `/`) and `**` as a full path
  component (matches any number of path components including zero). When `**`
  appears but is not a full path component (not surrounded by `/` or at
  start/end), it is treated as two `*` wildcards, which behave identically to
  one `*`.

  This is a pure string matcher with no I/O or ignore-file concepts.
  """

  fun matches(pattern: String val, text: String val): Bool =>
    """
    Return true if `text` matches the glob `pattern`.
    """
    _matches(pattern, 0, pattern.size(), text, 0, text.size())

  fun _matches(
    p: String val, ps: USize, pe: USize,
    t: String val, ts: USize, te: USize)
    : Bool
  =>
    """
    Recursive glob match with index ranges. Handles `**` decomposition, then
    delegates to `_match_segment` for `*`-only matching.
    """
    let plen = pe - ps

    // ** as entire pattern matches everything
    if (plen == 2) and _is_star(p, ps) and _is_star(p, ps + 1) then
      return true
    end

    // Leading **/
    if (plen >= 3) and _is_star(p, ps) and _is_star(p, ps + 1)
      and _is_slash(p, ps + 2)
    then
      let rest = ps + 3
      // Try rest against the whole text
      if _matches(p, rest, pe, t, ts, te) then return true end
      // Try rest against every suffix starting after each /
      var i = ts
      while i < te do
        if _is_slash(t, i) then
          if _matches(p, rest, pe, t, i + 1, te) then return true end
        end
        i = i + 1
      end
      return false
    end

    // Trailing /**
    if (plen >= 3) and _is_slash(p, pe - 3) and _is_star(p, pe - 2)
      and _is_star(p, pe - 1)
    then
      let prefix_end = pe - 3
      // Try prefix against text up to each / boundary; /** requires at least
      // a / after the prefix match, so we never try the full text as a prefix
      var i = te
      while i > ts do
        i = i - 1
        if _is_slash(t, i) then
          if _match_segment(p, ps, prefix_end, t, ts, i) then return true end
        end
      end
      return false
    end

    // Middle /**/
    var search = ps
    while (search + 3) < pe do
      if _is_slash(p, search) and _is_star(p, search + 1)
        and _is_star(p, search + 2) and _is_slash(p, search + 3)
      then
        let left_end = search
        let right_start = search + 4
        // For each / in text, try matching left against prefix and
        // right (with leading ** semantics) against the suffix
        var ti = ts
        while ti < te do
          if _is_slash(t, ti) then
            if _match_segment(p, ps, left_end, t, ts, ti) then
              if _match_leading_star(p, right_start, pe, t, ti + 1, te) then
                return true
              end
            end
          end
          ti = ti + 1
        end
        // left matches entire text and right is empty (only when pattern
        // ends with /**/ which would have been caught as trailing /**)
        return false
      end
      search = search + 1
    end

    // No ** as a path component — delegate to segment matching
    _match_segment(p, ps, pe, t, ts, te)

  fun _match_leading_star(
    p: String val, ps: USize, pe: USize,
    t: String val, ts: USize, te: USize)
    : Bool
  =>
    """
    Match with leading `**` semantics: try the pattern against the text and
    against every suffix starting after each `/`.
    """
    if _matches(p, ps, pe, t, ts, te) then return true end
    var i = ts
    while i < te do
      if _is_slash(t, i) then
        if _matches(p, ps, pe, t, i + 1, te) then return true end
      end
      i = i + 1
    end
    false

  fun _match_segment(
    p: String val, ps: USize, pe: USize,
    t: String val, ts: USize, te: USize)
    : Bool
  =>
    """
    Character-by-character matching with `*` backtracking. `*` matches any
    characters except `/`. Uses the standard iterative two-pointer algorithm
    with saved backtrack positions.
    """
    var pi = ps
    var ti = ts
    var star_pi: USize = USize.max_value()
    var star_ti: USize = USize.max_value()

    while ti < te do
      if (pi < pe) and _is_star(p, pi) then
        // * wildcard: save backtrack position, advance past * in pattern
        star_pi = pi
        star_ti = ti
        pi = pi + 1
      elseif (pi < pe) and _byte_eq(p, pi, t, ti) then
        // Literal match
        pi = pi + 1
        ti = ti + 1
      elseif star_pi != USize.max_value() then
        // Backtrack: let * consume one more character, but not /
        if _is_slash(t, star_ti) then
          return false
        end
        star_ti = star_ti + 1
        ti = star_ti
        pi = star_pi + 1
      else
        return false
      end
    end

    // Remaining pattern characters must all be *
    while pi < pe do
      if not _is_star(p, pi) then return false end
      pi = pi + 1
    end
    true

  fun _is_star(s: String val, i: USize): Bool =>
    try s(i)? == '*' else false end

  fun _is_slash(s: String val, i: USize): Bool =>
    try s(i)? == '/' else false end

  fun _byte_eq(a: String val, ai: USize, b: String val, bi: USize): Bool =>
    try a(ai)? == b(bi)? else false end
