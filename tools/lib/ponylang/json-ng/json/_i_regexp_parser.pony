class _IRegexpParser
  """
  Recursive descent parser for I-Regexp (RFC 9485).

  Produces a _RegexNode AST from a pattern string. Raises on invalid input;
  _IRegexpCompiler wraps this and converts errors to _IRegexpParseError.

  Structural scanning is byte-level (all metacharacters are ASCII). Literal
  codepoints are read via String.utf32 for full Unicode support.
  """
  let _source: String
  var _offset: USize = 0
  var _error_message: String = ""

  new create(source': String) =>
    _source = source'

  fun ref parse(): _RegexNode ? =>
    """Parse a complete I-Regexp. Raises on invalid input."""
    let result = _parse_alternation()?
    if _offset < _source.size() then
      _fail("Unexpected character")
      error
    end
    result

  fun error_result(): _IRegexpParseError =>
    _IRegexpParseError(_error_message, _offset)

  // --- Grammar productions ---

  fun ref _parse_alternation(): _RegexNode ? =>
    """i-regexp = branch *( "|" branch)"""
    var left = _parse_branch()?
    while _looking_at('|') do
      _advance(1)
      let right = _parse_branch()?
      left = _Alternation(left, right)
    end
    left

  fun ref _parse_branch(): _RegexNode ? =>
    """branch = *piece"""
    var pieces = Array[_RegexNode]
    while _at_atom_start() do
      pieces.push(_parse_piece()?)
    end
    match pieces.size()
    | 0 => _EmptyMatch
    | 1 => try pieces(0)? else _EmptyMatch end
    else
      // Left-fold into Concatenation tree
      var result = try pieces(0)? else _EmptyMatch end
      var i: USize = 1
      while i < pieces.size() do
        result = _Concatenation(result, try pieces(i)? else _EmptyMatch end)
        i = i + 1
      end
      result
    end

  fun ref _parse_piece(): _RegexNode ? =>
    """piece = atom [ quantifier ]"""
    let atom = _parse_atom()?
    if _at_quantifier() then
      _parse_quantifier(atom)?
    else
      atom
    end

  fun ref _parse_quantifier(base: _RegexNode): _RegexNode ? =>
    """quantifier = ("*" / "+" / "?") / range-quantifier"""
    if _looking_at('*') then
      _advance(1)
      _Quantified(base, 0, None)
    elseif _looking_at('+') then
      _advance(1)
      _Quantified(base, 1, None)
    elseif _looking_at('?') then
      _advance(1)
      _Quantified(base, 0, USize(1))
    elseif _looking_at('{') then
      _parse_range_quantifier(base)?
    else
      _fail("Expected quantifier")
      error
    end

  fun ref _parse_range_quantifier(base: _RegexNode): _RegexNode ? =>
    """range-quantifier = "{" QuantExact [ "," [ QuantExact ] ] "}" """
    _eat('{')?
    let min_val = _parse_quant_exact()?
    if _looking_at(',') then
      _advance(1)
      if _looking_at('}') then
        // {n,} — n or more
        _eat('}')?
        _Quantified(base, min_val, None)
      else
        // {n,m}
        let max_val = _parse_quant_exact()?
        _eat('}')?
        if max_val < min_val then
          _fail("Range quantifier max less than min")
          error
        end
        _Quantified(base, min_val, max_val)
      end
    else
      // {n} — exactly n
      _eat('}')?
      _Quantified(base, min_val, min_val)
    end

  fun ref _parse_quant_exact(): USize ? =>
    """QuantExact = 1*(%x30-39)"""
    if not _is_digit() then
      _fail("Expected digit in quantifier")
      error
    end
    var n: USize = 0
    while _is_digit() do
      try
        n = (n * 10) + (_source(_offset)? - '0').usize()
      end
      if n > 10_000 then
        _fail("Quantifier value too large")
        error
      end
      _advance(1)
    end
    n

  fun ref _parse_atom(): _RegexNode ? =>
    """atom = NormalChar / charClass / ( "(" i-regexp ")" )"""
    if _looking_at('(') then
      _advance(1)
      let inner = _parse_alternation()?
      _eat(')')?
      _Group(inner)
    elseif _looking_at('.') or _looking_at('\\') or _looking_at('[') then
      _parse_char_class()?
    else
      _parse_normal_char()?
    end

  fun ref _parse_char_class(): _RegexNode ? =>
    """charClass = "." / SingleCharEsc / charClassEsc / charClassExpr"""
    if _looking_at('.') then
      _advance(1)
      _Dot
    elseif _looking_at('[') then
      _parse_class_expr()?
    elseif _looking_at('\\') then
      _parse_escape()?
    else
      _fail("Expected character class")
      error
    end

  fun ref _parse_escape(): _RegexNode ? =>
    """Parse \x escape sequences: SingleCharEsc, catEsc, complEsc."""
    _eat('\\')?
    if _offset >= _source.size() then
      _fail("Unexpected end of pattern after backslash")
      error
    end
    try
      let c = _source(_offset)?
      if c == 'p' then
        (let ranges, let negated) = _parse_unicode_property(false)?
        _CharClass(ranges, negated)
      elseif c == 'P' then
        (let ranges, let negated) = _parse_unicode_property(true)?
        _CharClass(ranges, negated)
      elseif c == 'n' then
        _advance(1)
        _Literal(0x0A)
      elseif c == 'r' then
        _advance(1)
        _Literal(0x0D)
      elseif c == 't' then
        _advance(1)
        _Literal(0x09)
      elseif _is_escapable_metachar(c) then
        _advance(1)
        _Literal(c.u32())
      elseif (c == 'd') or (c == 'D') or (c == 'w') or (c == 'W')
        or (c == 's') or (c == 'S')
      then
        _fail("Multi-character escapes (\\d, \\w, \\s) are not part of I-Regexp"
          + " (RFC 9485). Use explicit character classes instead"
          + " (e.g., [0-9] for \\d, \\p{Nd} for Unicode digits).")
        error
      else
        _fail("Invalid escape sequence: \\" + String.from_array([c]))
        error
      end
    else
      _fail("Unexpected end of pattern after backslash")
      error
    end

  fun ref _parse_unicode_property(negate_outer: Bool)
    : (Array[(U32, U32)] val, Bool) ?
  =>
    """Parse \p{Name} or \P{Name}. negate_outer is true for \P."""
    _advance(1) // consume 'p' or 'P'
    _eat('{')?
    // Read category name (1-2 alphanumeric characters)
    let name_start = _offset
    while (_offset < _source.size()) and
      try
        let c = _source(_offset)?
        ((c >= 'A') and (c <= 'Z')) or ((c >= 'a') and (c <= 'z'))
      else
        false
      end
    do
      _advance(1)
    end
    let name_len = _offset - name_start
    if (name_len == 0) or (name_len > 2) then
      _fail("Invalid Unicode category name")
      error
    end
    let name = _source.substring(name_start.isize(), _offset.isize())
    _eat('}')?
    try
      let ranges = _UnicodeCategories(consume name)?
      (ranges, negate_outer)
    else
      _fail("Unknown Unicode category")
      error
    end

  fun ref _parse_class_expr(): _RegexNode ? =>
    """
    charClassExpr = "[" [ "^" ] ( "-" / CCE1 ) *CCE1 [ "-" ] "]"
    """
    _eat('[')?
    let negated = _looking_at('^')
    if negated then
      _advance(1)
      // [^] is explicitly forbidden by RFC 9485
      if _looking_at(']') then
        _fail("[^] is not allowed in I-Regexp (RFC 9485)")
        error
      end
    end
    let ranges = Array[(U32, U32)]
    // First element: "-" is literal, or CCE1
    if _looking_at('-') then
      _advance(1)
      ranges.push((U32('-'), U32('-')))
    else
      _parse_cce1(ranges)?
    end
    // Subsequent CCE1 elements
    while not _looking_at(']') do
      if _offset >= _source.size() then
        _fail("Unterminated character class")
        error
      end
      // Check for trailing "-" before "]"
      if _looking_at('-') then
        // Peek: is the next char ']'?
        if try _source(_offset + 1)? == ']' else false end then
          _advance(1) // consume '-'
          ranges.push((U32('-'), U32('-')))
        else
          _parse_cce1(ranges)?
        end
      else
        _parse_cce1(ranges)?
      end
    end
    _eat(']')?
    // Sort and merge ranges
    let merged_iso = recover iso Array[(U32, U32)](ranges.size()) end
    for r in ranges.values() do
      merged_iso.push(r)
    end
    _CharClass(_RangeOps.merge(consume merged_iso), negated)

  fun ref _parse_cce1(ranges: Array[(U32, U32)]) ? =>
    """
    CCE1 = ( CCchar [ "-" CCchar ] ) / charClassEsc
    """
    if _looking_at('\\') then
      // Could be SingleCharEsc or charClassEsc (\p{} / \P{})
      try
        let next = _source(_offset + 1)?
        if (next == 'p') or (next == 'P') then
          _eat('\\')?
          let negate = next == 'P'
          (let prop_ranges, _) = _parse_unicode_property(negate)?
          if negate then
            // \P{X} inside a character class: add complement ranges
            // For simplicity, add the negated ranges as-is and let caller
            // handle the negation at match time via _CharClass.negated
            // Actually, \P{X} inside [...] means "add codepoints NOT in X"
            // We need to compute the complement ranges and add them
            let complemented = _negate_ranges(prop_ranges)
            for r in complemented.values() do ranges.push(r) end
          else
            for r in prop_ranges.values() do ranges.push(r) end
          end
          return
        end
      end
      // SingleCharEsc
      _eat('\\')?
      let cp = _parse_single_char_esc()?
      // Check for range
      if _looking_at('-') and not _next_is_bracket() then
        _advance(1) // consume '-'
        let end_cp = _parse_cc_char()?
        if end_cp < cp then
          _fail("Character class range is reversed")
          error
        end
        ranges.push((cp, end_cp))
      else
        ranges.push((cp, cp))
      end
    else
      // CCchar
      let start_cp = _parse_cc_char()?
      if _looking_at('-') and not _next_is_bracket() then
        _advance(1) // consume '-'
        let end_cp = _parse_cc_char()?
        if end_cp < start_cp then
          _fail("Character class range is reversed")
          error
        end
        ranges.push((start_cp, end_cp))
      else
        ranges.push((start_cp, start_cp))
      end
    end

  fun ref _parse_cc_char(): U32 ? =>
    """
    CCchar = (%x00-2C / %x2E-5A / %x5E-D7FF / %xE000-10FFFF) / SingleCharEsc
    """
    if _looking_at('\\') then
      _eat('\\')?
      _parse_single_char_esc()?
    else
      if _offset >= _source.size() then
        _fail("Unexpected end of pattern in character class")
        error
      end
      // Read a Unicode codepoint
      try
        (let cp, let len) = _source.utf32(_offset.isize())?
        // RFC 9485 CCchar excludes '[' (0x5B) — must be escaped as \[
        if cp == U32('[') then
          _fail("'[' must be escaped inside character class")
          error
        end
        _offset = _offset + len.usize()
        cp
      else
        _fail("Invalid character in character class")
        error
      end
    end

  fun ref _parse_single_char_esc(): U32 ? =>
    """
    SingleCharEsc = "\\" (metachar / "n" / "r" / "t")
    Backslash already consumed by caller.
    """
    if _offset >= _source.size() then
      _fail("Unexpected end of pattern after backslash")
      error
    end
    try
      let c = _source(_offset)?
      _advance(1)
      match c
      | 'n' => U32(0x0A)
      | 'r' => U32(0x0D)
      | 't' => U32(0x09)
      else
        if _is_escapable_metachar(c) then
          c.u32()
        else
          _fail("Invalid escape sequence in character class")
          error
        end
      end
    else
      _fail("Unexpected end of pattern")
      error
    end

  fun ref _parse_normal_char(): _RegexNode ? =>
    """Parse a NormalChar (any Unicode codepoint that is not a metacharacter)."""
    if _offset >= _source.size() then
      _fail("Unexpected end of pattern")
      error
    end
    try
      (let cp, let len) = _source.utf32(_offset.isize())?
      // Check that it's not a metacharacter (all metacharacters are ASCII)
      if (cp < 0x80) and _is_metachar(cp.u8()) then
        _fail("Unexpected metacharacter: " + String.from_utf32(cp))
        error
      end
      _offset = _offset + len.usize()
      _Literal(cp)
    else
      _fail("Invalid character encoding")
      error
    end

  // --- Character classification helpers ---

  fun _is_metachar(c: U8): Bool =>
    """Check if a byte is an I-Regexp metacharacter."""
    (c == '(') or (c == ')') or (c == '*') or (c == '+') or
    (c == '.') or (c == '?') or (c == '[') or (c == '\\') or
    (c == ']') or (c == '^') or (c == '{') or (c == '|') or (c == '}')

  fun _is_escapable_metachar(c: U8): Bool =>
    """Check if a byte can appear after backslash as a SingleCharEsc."""
    // RFC 9485 SingleCharEsc: ( %x28-2B / "-" / "." / "?" /
    //   %x5B-5E / "n" / "r" / "t" / %x7B-7D )
    // Note: n, r, t are handled separately by caller
    (c == '(') or (c == ')') or (c == '*') or (c == '+') or  // %x28-2B
    (c == '-') or (c == '.') or (c == '?') or                // -, ., ?
    (c == '[') or (c == '\\') or (c == ']') or (c == '^') or // %x5B-5E
    (c == '{') or (c == '|') or (c == '}')                   // %x7B-7D

  fun _at_atom_start(): Bool =>
    """Check if current position could be the start of an atom."""
    if _offset >= _source.size() then return false end
    try
      let c = _source(_offset)?
      // Not at atom start if at: ), |, *, +, ?, {, }, ], or end
      not ((c == ')') or (c == '|') or (c == '*') or (c == '+') or
           (c == '?') or (c == '{') or (c == '}') or (c == ']'))
    else
      false
    end

  fun _at_quantifier(): Bool =>
    """Check if current position is a quantifier start."""
    if _offset >= _source.size() then return false end
    try
      let c = _source(_offset)?
      (c == '*') or (c == '+') or (c == '?') or (c == '{')
    else
      false
    end

  fun _next_is_bracket(): Bool =>
    """Check if the next position (after current) is ']'."""
    try _source(_offset + 1)? == ']' else false end

  fun _negate_ranges(ranges: Array[(U32, U32)] val): Array[(U32, U32)] val =>
    """Compute the complement of ranges over valid Unicode scalar values."""
    // Valid: 0x0000-0xD7FF, 0xE000-0x10FFFF
    let result = recover iso Array[(U32, U32)] end
    var pos: U32 = 0
    for (range_start, range_end) in ranges.values() do
      if range_start > pos then
        // Gap before this range
        if (pos <= 0xD7FF) and (range_start > 0) then
          let gap_end = range_start - 1
          // Split around surrogate range
          if (pos <= 0xD7FF) and (gap_end >= 0xD800) then
            result.push((pos, 0xD7FF))
            if gap_end >= 0xE000 then
              result.push((0xE000, gap_end))
            end
          elseif pos >= 0xE000 then
            result.push((pos, gap_end))
          elseif gap_end < 0xD800 then
            result.push((pos, gap_end))
          end
        elseif pos >= 0xE000 then
          result.push((pos, range_start - 1))
        end
      end
      pos = range_end + 1
    end
    // Trailing gap
    if pos <= 0x10FFFF then
      if (pos <= 0xD7FF) then
        result.push((pos, 0xD7FF))
        result.push((0xE000, 0x10FFFF))
      elseif (pos >= 0xD800) and (pos <= 0xDFFF) then
        result.push((0xE000, 0x10FFFF))
      else
        result.push((pos, 0x10FFFF))
      end
    end
    consume result

  // --- Character primitives ---

  fun _looking_at(c: U8): Bool =>
    try _source(_offset)? == c else false end

  fun ref _advance(n: USize) =>
    _offset = _offset + n

  fun ref _eat(expected: U8) ? =>
    if _offset >= _source.size() then
      _fail("Expected '" + String.from_array([expected]) +
        "' but reached end of pattern")
      error
    end
    if try _source(_offset)? != expected else true end then
      _fail("Expected '" + String.from_array([expected]) + "'")
      error
    end
    _offset = _offset + 1

  fun _is_digit(): Bool =>
    try
      let c = _source(_offset)?
      (c >= '0') and (c <= '9')
    else
      false
    end

  fun ref _fail(msg: String) =>
    _error_message = msg
