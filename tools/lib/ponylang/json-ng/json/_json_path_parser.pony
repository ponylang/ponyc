class ref _JsonPathParser
  """
  Internal recursive descent parser for JSONPath expressions.

  Raises on invalid input. The public JsonPathParser.parse() wraps
  this and catches errors, consistent with how JsonParser wraps
  JsonTokenParser.
  """
  let _source: String
  var _offset: USize = 0
  var _error_message: String = ""

  new ref create(source': String) =>
    _source = source'

  fun ref parse(): Array[_Segment] val ? =>
    """Parse a complete JSONPath expression. Raises on invalid input."""
    _eat('$')?
    let segments = recover iso Array[_Segment] end
    while _offset < _source.size() do
      segments.push(_parse_segment()?)
    end
    consume segments

  fun error_result(): JsonPathParseError =>
    """Return a parse error with current position context."""
    JsonPathParseError(_error_message, _offset)

  // --- Segment parsing ---

  fun ref _parse_segment(): _Segment ? =>
    """Parse a child or descendant segment."""
    if _looking_at_str("..") then
      _advance(2)
      _parse_descendant_segment()?
    elseif _looking_at('.') then
      _advance(1)
      _parse_dot_child()?
    elseif _looking_at('[') then
      _parse_bracket_child()?
    else
      _fail("Expected '.', '..', or '['")
      error
    end

  fun ref _parse_descendant_segment(): _Segment ? =>
    """Parse after '..' — either bracket selectors or dot member/wildcard."""
    let selectors = if _looking_at('[') then
      _parse_bracket_selectors()?
    elseif _looking_at('*') then
      _advance(1)
      recover val [as _Selector: _WildcardSelector] end
    else
      let name = _parse_member_name()?
      recover val [as _Selector: _NameSelector(name)] end
    end
    _DescendantSegment(selectors)

  fun ref _parse_dot_child(): _Segment ? =>
    """Parse after '.' — either wildcard or member name."""
    let selectors = if _looking_at('*') then
      _advance(1)
      recover val [as _Selector: _WildcardSelector] end
    else
      let name = _parse_member_name()?
      recover val [as _Selector: _NameSelector(name)] end
    end
    _ChildSegment(selectors)

  fun ref _parse_bracket_child(): _Segment ? =>
    """Parse bracket notation '[selectors]' as a child segment."""
    let selectors = _parse_bracket_selectors()?
    _ChildSegment(selectors)

  // --- Bracket selector parsing ---

  fun ref _parse_bracket_selectors(): Array[_Selector] val ? =>
    """Parse '[' selector (',' selector)* ']'."""
    _eat('[')?
    _skip_whitespace()
    let selectors = recover iso Array[_Selector] end
    selectors.push(_parse_selector()?)
    while true do
      _skip_whitespace()
      if _looking_at(']') then
        _advance(1)
        break
      elseif _looking_at(',') then
        _advance(1)
        _skip_whitespace()
        selectors.push(_parse_selector()?)
      else
        _fail("Expected ',' or ']' in bracket selector")
        error
      end
    end
    consume selectors

  fun ref _parse_selector(): _Selector ? =>
    """
    Parse a single selector inside brackets.
    Distinguishes: filter, string name, wildcard, index, or slice.
    """
    _skip_whitespace()
    if _looking_at('?') then
      _parse_filter_selector()?
    elseif _looking_at('*') then
      _advance(1)
      _WildcardSelector
    elseif _looking_at('\'') or _looking_at('"') then
      let name = _parse_quoted_string()?
      _NameSelector(name)
    else
      _parse_index_or_slice()?
    end

  fun ref _parse_index_or_slice(): _Selector ? =>
    """
    Parse an integer index or a slice expression.

    Disambiguation: if we see ':' after the optional first integer,
    it's a slice; otherwise it's an index. A second ':' introduces
    the optional step value.
    """
    let first: (I64 | None) = _try_parse_int()

    _skip_whitespace()
    if _looking_at(':') then
      _advance(1)
      _skip_whitespace()
      let end_val: (I64 | None) = _try_parse_int()
      _skip_whitespace()
      let step_val: (I64 | None) = if _looking_at(':') then
        _advance(1)
        _skip_whitespace()
        _try_parse_int()
      else
        None
      end
      _SliceSelector(first, end_val, step_val)
    else
      match first
      | let n: I64 => _IndexSelector(n)
      else
        _fail("Expected integer index, string name, or ':' for slice")
        error
      end
    end

  // --- Leaf parsing ---

  fun ref _parse_member_name(): String ? =>
    """Parse an unquoted member name (dot notation)."""
    let start = _offset
    if _offset >= _source.size() then
      _fail("Expected member name")
      error
    end
    let first = _source(_offset)?
    if not (_is_alpha(first) or (first == '_')) then
      _fail("Expected letter or '_' at start of member name")
      error
    end
    _advance(1)
    while _offset < _source.size() do
      let c = _source(_offset)?
      if _is_alpha(c) or _is_digit(c) or (c == '_') then
        _advance(1)
      else
        break
      end
    end
    _source.substring(start.isize(), _offset.isize())

  fun ref _parse_quoted_string(): String ? =>
    """Parse a single- or double-quoted string with escape handling."""
    let quote = _next()?
    let buf = String
    while true do
      if _offset >= _source.size() then
        _fail("Unterminated string")
        error
      end
      let c = _next()?
      if c == quote then
        break
      elseif c == '\\' then
        if _offset >= _source.size() then
          _fail("Unterminated escape sequence")
          error
        end
        let esc = _next()?
        match esc
        | '"' => buf.push('"')
        | '\'' => buf.push('\'')
        | '\\' => buf.push('\\')
        | '/' => buf.push('/')
        | 'b' => buf.push(0x08)
        | 'f' => buf.push(0x0C)
        | 'n' => buf.push('\n')
        | 'r' => buf.push('\r')
        | 't' => buf.push('\t')
        | 'u' => _parse_unicode_escape(buf)?
        else
          _fail("Invalid escape sequence")
          error
        end
      else
        buf.push(c)
      end
    end
    buf.clone()

  fun ref _parse_unicode_escape(buf: String ref) ? =>
    """Parse \\uXXXX and surrogate pairs, appending to buf."""
    let value = _read_hex4()?
    if (value >= 0xD800) and (value < 0xDC00) then
      // High surrogate — expect \uXXXX low surrogate
      _eat('\\')?
      _eat('u')?
      let low = _read_hex4()?
      if (low >= 0xDC00) and (low < 0xE000) then
        let combined =
          0x10000 + (((value and 0x3FF) << 10) or (low and 0x3FF))
        buf.append(recover val String.from_utf32(combined) end)
      else
        _fail("Invalid surrogate pair")
        error
      end
    elseif (value >= 0xDC00) and (value < 0xE000) then
      _fail("Lone low surrogate")
      error
    else
      buf.append(recover val String.from_utf32(value) end)
    end

  fun ref _read_hex4(): U32 ? =>
    """Read exactly 4 hex digits and return the value."""
    var result: U32 = 0
    var i: USize = 0
    while i < 4 do
      let c = _next()?
      let digit: U32 = if (c >= '0') and (c <= '9') then
        (c - '0').u32()
      elseif (c >= 'a') and (c <= 'f') then
        (c - 'a').u32() + 10
      elseif (c >= 'A') and (c <= 'F') then
        (c - 'A').u32() + 10
      else
        _fail("Invalid hex digit")
        error
      end
      result = (result << 4) or digit
      i = i + 1
    end
    result

  fun ref _try_parse_int(): (I64 | None) =>
    """Try to parse an integer. Returns None if not at a digit or '-'."""
    if _offset >= _source.size() then return None end
    try
      let c = _source(_offset)?
      if _is_digit(c) or (c == '-') then
        _parse_int()?
      else
        None
      end
    else
      None
    end

  fun ref _parse_int(): I64 ? =>
    """Parse a (possibly negative) integer."""
    var negative = false
    if _looking_at('-') then
      negative = true
      _advance(1)
    end
    let start = _offset
    while (_offset < _source.size()) and
      try _is_digit(_source(_offset)?) else false end
    do
      _advance(1)
    end
    if _offset == start then
      _fail("Expected digit")
      error
    end
    let num_str: String val =
      _source.substring(start.isize(), _offset.isize())
    let abs_val = num_str.i64()?
    if negative then -abs_val else abs_val end

  // --- Filter expression parsing ---

  fun ref _parse_filter_selector(): _FilterSelector ? =>
    """Parse a filter selector: '?' logical-expr."""
    _advance(1) // consume '?'
    _skip_whitespace()
    let expr = _parse_logical_or_expr()?
    _FilterSelector(expr)

  fun ref _parse_logical_or_expr(): _LogicalExpr ? =>
    """Parse logical-or: logical-and *('||' logical-and)."""
    var left = _parse_logical_and_expr()?
    while true do
      _skip_whitespace()
      if _looking_at_str("||") then
        _advance(2)
        _skip_whitespace()
        let right = _parse_logical_and_expr()?
        left = _OrExpr(left, right)
      else
        break
      end
    end
    left

  fun ref _parse_logical_and_expr(): _LogicalExpr ? =>
    """Parse logical-and: basic-expr *('&&' basic-expr)."""
    var left = _parse_basic_expr()?
    while true do
      _skip_whitespace()
      if _looking_at_str("&&") then
        _advance(2)
        _skip_whitespace()
        let right = _parse_basic_expr()?
        left = _AndExpr(left, right)
      else
        break
      end
    end
    left

  fun ref _parse_basic_expr(): _LogicalExpr ? =>
    """
    Parse a basic expression: negation, parenthesized, function call,
    query-based (test or comparison), or literal-first comparison.
    """
    _skip_whitespace()
    if _looking_at('!') then
      _advance(1)
      _skip_whitespace()
      if _looking_at('(') then
        // Negated parenthesized expression
        _NotExpr(_parse_paren_expr()?)
      elseif _looking_at('@') or _looking_at('$') then
        // Negated existence test
        let is_rel = _looking_at('@')
        _advance(1)
        let segments = _parse_filter_segments()?
        let query: _FilterQuery = if is_rel then
          _RelFilterQuery(segments)
        else
          _AbsFilterQuery(segments)
        end
        _NotExpr(_ExistenceExpr(query))
      elseif _looking_at_function_name() or _looking_at_general_function() then
        // Negated function call — only LogicalType functions allowed
        let func = _parse_function_call()?
        match func
        | let m: _MatchExpr => _NotExpr(m)
        | let s: _SearchExpr => _NotExpr(s)
        else
          _fail(
            "Cannot negate value-returning function"
            + " — only match() and search() can be negated")
          error
        end
      else
        _fail("Expected '(', '@', '$', or function name after '!'")
        error
      end
    elseif _looking_at('(') then
      _parse_paren_expr()?
    elseif _looking_at('@') or _looking_at('$') then
      _parse_test_or_comparison()?
    elseif _looking_at_function_name() or _looking_at_general_function() then
      // Function call — LogicalType as test-expr, ValueType as comparison LHS
      let func = _parse_function_call()?
      match func
      | let m: _MatchExpr => m
      | let s: _SearchExpr => s
      else
        // ValueType function — must be left side of a comparison
        let left: _Comparable = match func
        | let l: _LengthExpr => l
        | let c: _CountExpr => c
        | let v: _ValueExpr => v
        else
          _Unreachable(__loc)
          error
        end
        _skip_whitespace()
        if not _looking_at_comparison_op() then
          _fail(
            "Value-returning function requires a comparison operator"
            + " (e.g., length(@.a) > 3)")
          error
        end
        let op = _parse_comparison_op()?
        _skip_whitespace()
        let right = _parse_comparable()?
        _ComparisonExpr(left, op, right)
      end
    else
      // Must be a literal-first comparison: literal op comparable
      let left: _Comparable = _parse_literal()?
      _skip_whitespace()
      let op = _parse_comparison_op()?
      _skip_whitespace()
      let right = _parse_comparable()?
      _ComparisonExpr(left, op, right)
    end

  fun ref _parse_paren_expr(): _LogicalExpr ? =>
    """Parse '(' logical-expr ')'."""
    _eat('(')?
    _skip_whitespace()
    let expr = _parse_logical_or_expr()?
    _skip_whitespace()
    _eat(')')?
    expr

  fun ref _parse_test_or_comparison(): _LogicalExpr ? =>
    """
    Disambiguate test-expr vs comparison-expr starting with @ or $.

    Strategy: save position, try parsing singular segments (name/index
    only). If a comparison operator follows, it's a comparison. Otherwise,
    reset and re-parse as general filter segments for an existence test.
    """
    let is_rel = _looking_at('@')
    _advance(1) // consume @ or $
    let segments_start = _offset

    // Try parsing as singular query + comparison op
    try
      let singular_segs = _parse_singular_segments()?
      _skip_whitespace()
      if _looking_at_comparison_op() then
        // It's a comparison
        let query: _SingularQuery = if is_rel then
          _RelSingularQuery(singular_segs)
        else
          _AbsSingularQuery(singular_segs)
        end
        let left: _Comparable = query
        let op = _parse_comparison_op()?
        _skip_whitespace()
        let right = _parse_comparable()?
        return _ComparisonExpr(left, op, right)
      end
    end

    // Not a comparison — re-parse as general filter query for existence test
    _offset = segments_start
    let segments = _parse_filter_segments()?
    let query: _FilterQuery = if is_rel then
      _RelFilterQuery(segments)
    else
      _AbsFilterQuery(segments)
    end
    _ExistenceExpr(query)

  fun ref _parse_comparable(): _Comparable ? =>
    """Parse a comparable: singular query, function expression, or literal."""
    _skip_whitespace()
    if _looking_at('@') then
      _advance(1)
      let segs = _parse_singular_segments()?
      _RelSingularQuery(segs)
    elseif _looking_at('$') then
      _advance(1)
      let segs = _parse_singular_segments()?
      _AbsSingularQuery(segs)
    elseif _looking_at_function_name() or _looking_at_general_function() then
      // Only ValueType functions are valid as comparables
      let func = _parse_function_call()?
      match func
      | let l: _LengthExpr => l
      | let c: _CountExpr => c
      | let v: _ValueExpr => v
      else
        _fail("match() and search() cannot be used in comparisons")
        error
      end
    else
      _parse_literal()?
    end

  fun ref _parse_literal(): _LiteralValue ? =>
    """Parse a literal value: string, number, true, false, or null."""
    _skip_whitespace()
    if _looking_at('\'') or _looking_at('"') then
      _parse_quoted_string()?
    elseif _looking_at_str("true") then
      _advance(4)
      true
    elseif _looking_at_str("false") then
      _advance(5)
      false
    elseif _looking_at_str("null") then
      _advance(4)
      None
    else
      _parse_json_number()?
    end

  fun ref _parse_json_number(): _LiteralValue ? =>
    """Parse a full JSON number: ['-'] int ['.' digits] [('e'|'E') [sign] digits]."""
    let start = _offset
    var is_float = false

    // Optional negative sign
    if _looking_at('-') then _advance(1) end

    // Integer part
    if _looking_at('0') then
      _advance(1)
    else
      if _offset >= _source.size() then
        _fail("Expected digit")
        error
      end
      let c = _source(_offset)?
      if not _is_digit(c) then
        _fail("Expected digit")
        error
      end
      _advance(1)
      while (_offset < _source.size()) and
        try _is_digit(_source(_offset)?) else false end
      do
        _advance(1)
      end
    end

    // Optional fraction
    if _looking_at('.') then
      is_float = true
      _advance(1)
      let frac_start = _offset
      while (_offset < _source.size()) and
        try _is_digit(_source(_offset)?) else false end
      do
        _advance(1)
      end
      if _offset == frac_start then
        _fail("Expected digit after decimal point")
        error
      end
    end

    // Optional exponent
    if try
      let c = _source(_offset)?
      (c == 'e') or (c == 'E')
    else false end then
      is_float = true
      _advance(1)
      if _looking_at('+') or _looking_at('-') then _advance(1) end
      let exp_start = _offset
      while (_offset < _source.size()) and
        try _is_digit(_source(_offset)?) else false end
      do
        _advance(1)
      end
      if _offset == exp_start then
        _fail("Expected digit in exponent")
        error
      end
    end

    let num_str: String val =
      _source.substring(start.isize(), _offset.isize())
    if is_float then
      let v: _LiteralValue = num_str.f64()?
      v
    else
      let v: _LiteralValue = num_str.i64()?
      v
    end

  fun ref _parse_singular_segments(): Array[_SingularSegment] val ? =>
    """
    Parse segments restricted to name and index only (no wildcards,
    slices, descendants, or filters). Raises on non-singular syntax.
    """
    let segs = recover iso Array[_SingularSegment] end
    while _offset < _source.size() do
      if _looking_at('.') and (not _looking_at_str("..")) then
        _advance(1)
        let name = _parse_member_name()?
        segs.push(_SingularNameSegment(name))
      elseif _looking_at('[') then
        _advance(1)
        _skip_whitespace()
        if _looking_at('\'') or _looking_at('"') then
          let name = _parse_quoted_string()?
          _skip_whitespace()
          _eat(']')?
          segs.push(_SingularNameSegment(name))
        else
          let idx = _parse_int()?
          _skip_whitespace()
          // Must be ] — if it's : then it's a slice (not singular)
          _eat(']')?
          segs.push(_SingularIndexSegment(idx))
        end
      else
        break
      end
    end
    consume segs

  fun ref _parse_filter_segments(): Array[_Segment] val ? =>
    """
    Parse general segments for filter queries (existence tests).

    Stops when the next character is not '.' or '['. Bracket selectors
    within the query (e.g., `@.items[0]`) are consumed in full by the
    existing `_parse_bracket_selectors`, so nested ']' does not
    prematurely terminate parsing.
    """
    let segments = recover iso Array[_Segment] end
    while _offset < _source.size() do
      _skip_whitespace()
      if not (_looking_at('.') or _looking_at('[')) then break end
      segments.push(_parse_segment()?)
    end
    consume segments

  // --- Function extension parsing (RFC 9535 Section 2.4) ---

  fun _looking_at_function_name(): Bool =>
    """Check if current position starts with a known function name + '('."""
    _looking_at_str("length(") or _looking_at_str("count(") or
    _looking_at_str("match(") or _looking_at_str("search(") or
    _looking_at_str("value(")

  fun _looking_at_general_function(): Bool =>
    """
    Check if current position starts with any lowercase identifier + '('.
    Used to produce clear "Unknown function" errors when the name
    isn't one of the 5 built-in functions.
    """
    try
      let first = _source(_offset)?
      if not ((first >= 'a') and (first <= 'z')) then return false end
      // Scan past the identifier to find '('
      var i = _offset + 1
      while i < _source.size() do
        let c = _source(i)?
        if ((c >= 'a') and (c <= 'z')) or _is_digit(c) or (c == '_') then
          i = i + 1
        else
          return c == '('
        end
      end
      false
    else
      false
    end

  fun ref _parse_function_name(): String ? =>
    """
    Consume and return a function name identifier, or fail with a clear
    error if the name isn't one of the 5 built-in functions.
    """
    let start = _offset
    while _offset < _source.size() do
      try
        let c = _source(_offset)?
        if ((c >= 'a') and (c <= 'z')) or _is_digit(c) or (c == '_') then
          _advance(1)
        else
          break
        end
      else
        break
      end
    end
    let name: String val =
      _source.substring(start.isize(), _offset.isize())
    match name
    | "length" | "count" | "match" | "search" | "value" => name
    else
      _fail("Unknown function: " + name)
      error
    end

  fun ref _parse_function_call()
    : (_MatchExpr | _SearchExpr | _LengthExpr | _CountExpr | _ValueExpr) ?
  =>
    """
    Parse a function call: name '(' args ')'.

    Dispatches by function name to parse the correct argument types
    per RFC 9535 Section 2.4.
    """
    let name = _parse_function_name()?
    _eat('(')?
    _skip_whitespace()
    let result = match name
    | "length" =>
      let arg = _parse_comparable()?
      let r: (_MatchExpr | _SearchExpr | _LengthExpr
        | _CountExpr | _ValueExpr) = _LengthExpr(arg)
      r
    | "count" =>
      let query = _parse_filter_query_arg()?
      let r: (_MatchExpr | _SearchExpr | _LengthExpr
        | _CountExpr | _ValueExpr) = _CountExpr(query)
      r
    | "match" =>
      let input = _parse_comparable()?
      _skip_whitespace()
      _eat(',')?
      _skip_whitespace()
      let pattern = _parse_comparable()?
      let r: (_MatchExpr | _SearchExpr | _LengthExpr
        | _CountExpr | _ValueExpr) = _MatchExpr(input, pattern)
      r
    | "search" =>
      let input = _parse_comparable()?
      _skip_whitespace()
      _eat(',')?
      _skip_whitespace()
      let pattern = _parse_comparable()?
      let r: (_MatchExpr | _SearchExpr | _LengthExpr
        | _CountExpr | _ValueExpr) = _SearchExpr(input, pattern)
      r
    | "value" =>
      let query = _parse_filter_query_arg()?
      let r: (_MatchExpr | _SearchExpr | _LengthExpr
        | _CountExpr | _ValueExpr) = _ValueExpr(query)
      r
    else
      _fail("Unknown function: " + name)
      error
    end
    _skip_whitespace()
    _eat(')')?
    result

  fun ref _parse_filter_query_arg(): _FilterQuery ? =>
    """
    Parse a NodesType function argument: a filter query starting with
    '@' or '$' followed by segments.
    """
    let is_rel = if _looking_at('@') then
      _advance(1); true
    elseif _looking_at('$') then
      _advance(1); false
    else
      _fail("Expected '@' or '$' for query argument")
      error
    end
    let segments = _parse_filter_segments()?
    if is_rel then _RelFilterQuery(segments) else _AbsFilterQuery(segments) end

  fun ref _parse_comparison_op(): _ComparisonOp ? =>
    """Parse a comparison operator: ==, !=, <=, >=, <, >."""
    // Check two-char operators before single-char
    if _looking_at_str("==") then _advance(2); _CmpEq
    elseif _looking_at_str("!=") then _advance(2); _CmpNeq
    elseif _looking_at_str("<=") then _advance(2); _CmpLte
    elseif _looking_at_str(">=") then _advance(2); _CmpGte
    elseif _looking_at('<') then _advance(1); _CmpLt
    elseif _looking_at('>') then _advance(1); _CmpGt
    else
      _fail("Expected comparison operator")
      error
    end

  fun _looking_at_comparison_op(): Bool =>
    """Check if the current position starts with a comparison operator."""
    _looking_at_str("==") or _looking_at_str("!=") or
    _looking_at_str("<=") or _looking_at_str(">=") or
    _looking_at('<') or _looking_at('>')

  // --- Character primitives ---

  fun _looking_at(c: U8): Bool =>
    try _source(_offset)? == c else false end

  fun _looking_at_str(s: String): Bool =>
    if (_offset + s.size()) > _source.size() then return false end
    try
      var i: USize = 0
      while i < s.size() do
        if _source(_offset + i)? != s(i)? then return false end
        i = i + 1
      end
      true
    else
      false
    end

  fun ref _advance(n: USize) =>
    _offset = _offset + n

  fun ref _next(): U8 ? =>
    if _offset >= _source.size() then
      _fail("Unexpected end of path")
      error
    end
    let c = _source(_offset)?
    _offset = _offset + 1
    c

  fun ref _eat(expected: U8) ? =>
    if _offset >= _source.size() then
      _fail("Expected '" + String.from_array([expected]) +
        "' but reached end of path")
      error
    end
    if _source(_offset)? != expected then
      _fail("Expected '" + String.from_array([expected]) + "'")
      error
    end
    _offset = _offset + 1

  fun ref _skip_whitespace() =>
    while (_offset < _source.size()) and
      try
        let c = _source(_offset)?
        (c == ' ') or (c == '\t') or (c == '\n') or (c == '\r')
      else
        false
      end
    do
      _offset = _offset + 1
    end

  fun ref _fail(msg: String) =>
    _error_message = msg

  fun _is_alpha(c: U8): Bool =>
    ((c >= 'a') and (c <= 'z')) or ((c >= 'A') and (c <= 'Z'))

  fun _is_digit(c: U8): Bool =>
    (c >= '0') and (c <= '9')
