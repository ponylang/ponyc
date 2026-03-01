class ref JsonTokenParser
  """
  Streaming JSON token parser.

  Parses a JSON string and emits tokens to a JsonTokenNotify callback.
  This is the lower-level API — most users should use JsonParser.parse()
  instead. Use this when you need to process large documents without
  materializing the full tree.
  """

  var _notify: JsonTokenNotify
  var _source: String box = ""
  var _offset: USize = 0
  var _token_start: USize = 0
  var _line: USize = 1
  var _abort: Bool = false

  var last_number: (I64 | F64) = I64(0)
    """The most recently parsed number value."""

  var last_string: String = ""
    """The most recently parsed string or key value."""

  new ref create(notify': JsonTokenNotify) =>
    _notify = notify'

  fun ref parse(source': String box) ? =>
    """Parse a JSON document, emitting tokens to the notify callback."""
    _source = source'
    _offset = 0
    _token_start = 0
    _line = 1
    _abort = false
    last_number = I64(0)
    last_string = ""

    _skip_whitespace()
    if _offset >= _source.size() then return end
    _parse_value()?
    _skip_whitespace()
    if _offset < _source.size() then error end

  fun ref abort() =>
    """Signal the parser to stop after the current token."""
    _abort = true

  fun token_start(): USize =>
    """Byte offset where the current token starts."""
    _token_start

  fun token_end(): USize =>
    """Byte offset where the current token ends."""
    _offset

  fun line(): USize =>
    """Current line number (1-based)."""
    _line

  fun describe_error(): String =>
    """Human-readable description of the error location."""
    if _offset < _source.size() then
      "Invalid JSON at byte offset " + _offset.string()
        + ", line " + _line.string()
    else
      "Unexpected end of JSON at byte offset "
        + _source.size().string()
        + ", line " + _line.string()
    end

  // --- Private parsing methods ---

  fun ref _emit(token: JsonToken) ? =>
    _notify(this, token)
    if _abort then error end

  fun ref _parse_value() ? =>
    _skip_whitespace()
    _token_start = _offset
    match _peek()?
    | '{' => _parse_object()?
    | '[' => _parse_array()?
    | '"' => _parse_string(false)?
    | 't' => _parse_true()?
    | 'f' => _parse_false()?
    | 'n' => _parse_null()?
    | let c: U8 if (c == '-') or ((c >= '0') and (c <= '9')) =>
      _parse_number()?
    else
      error
    end

  fun ref _parse_object() ? =>
    _next()? // consume '{'
    _emit(JsonTokenObjectStart)?
    _skip_whitespace()

    if _peek()? == '}' then
      _next()?
      _emit(JsonTokenObjectEnd)?
      return
    end

    while true do
      _skip_whitespace()
      _token_start = _offset
      _parse_string(true)? // parse key
      _skip_whitespace()
      _eat(':')?
      _parse_value()? // parse value
      _skip_whitespace()
      _token_start = _offset
      match _next()?
      | ',' => None
      | '}' =>
        _emit(JsonTokenObjectEnd)?
        return
      else error
      end
    end

  fun ref _parse_array() ? =>
    _next()? // consume '['
    _emit(JsonTokenArrayStart)?
    _skip_whitespace()

    if _peek()? == ']' then
      _next()?
      _emit(JsonTokenArrayEnd)?
      return
    end

    while true do
      _parse_value()?
      _skip_whitespace()
      _token_start = _offset
      match _next()?
      | ',' => None
      | ']' =>
        _emit(JsonTokenArrayEnd)?
        return
      else error
      end
    end

  fun ref _parse_true() ? =>
    _eat('t')?; _eat('r')?; _eat('u')?; _eat('e')?
    _emit(JsonTokenTrue)?

  fun ref _parse_false() ? =>
    _eat('f')?; _eat('a')?; _eat('l')?; _eat('s')?; _eat('e')?
    _emit(JsonTokenFalse)?

  fun ref _parse_null() ? =>
    _eat('n')?; _eat('u')?; _eat('l')?; _eat('l')?
    _emit(JsonTokenNull)?

  fun ref _parse_number() ? =>
    let sign: I64 = if _peek_safe() == '-' then _next()?; -1 else 1 end
    let int_start = _offset
    let integer = _read_digits()?
    let int_digits = _offset - int_start

    // RFC 8259: leading zeros not allowed (e.g., 01, 00, 007)
    if try (_source(int_start)? == '0') and (int_digits > 1)
    else false end then
      error
    end

    // For large integers, re-read as F64 to get the correct value
    // (_read_digits accumulates into I64 which silently wraps on overflow)
    let force_float = int_digits > 18
    let integer_f64: F64 = if force_float then
      _offset = int_start
      _read_digits_f64()?
    else
      integer.f64()
    end

    var has_dot = false
    var frac: F64 = 0
    if _peek_safe() == '.' then
      _next()?
      has_dot = true
      frac = _read_fractional()?
    end

    var has_exp = false
    var exp: I64 = 0
    match _peek_safe()
    | 'e' | 'E' =>
      _next()?
      has_exp = true
      let exp_sign: I64 = match _peek()?
      | '+' => _next()?; 1
      | '-' => _next()?; -1
      else 1
      end
      exp = _read_digits()? * exp_sign
    end

    if has_dot or has_exp or force_float then
      last_number = sign.f64() * (integer_f64 + frac)
        * F64(10).pow(exp.f64())
    else
      last_number = sign * integer
    end
    _emit(JsonTokenNumber)?

  fun ref _read_digits(): I64 ? =>
    var result: I64 = 0
    var count: USize = 0
    while _offset < _source.size() do
      let c = _source(_offset)?
      if (c >= '0') and (c <= '9') then
        result = (result * 10) + (c - '0').i64()
        _offset = _offset + 1
        count = count + 1
      else
        break
      end
    end
    if count == 0 then error end
    result

  fun ref _read_digits_f64(): F64 ? =>
    var result: F64 = 0
    var count: USize = 0
    while _offset < _source.size() do
      let c = _source(_offset)?
      if (c >= '0') and (c <= '9') then
        result = (result * 10) + (c - '0').f64()
        _offset = _offset + 1
        count = count + 1
      else
        break
      end
    end
    if count == 0 then error end
    result

  fun ref _read_fractional(): F64 ? =>
    var result: F64 = 0
    var divisor: F64 = 10
    var count: USize = 0
    while _offset < _source.size() do
      let c = _source(_offset)?
      if (c >= '0') and (c <= '9') then
        result = result + ((c - '0').f64() / divisor)
        divisor = divisor * 10
        _offset = _offset + 1
        count = count + 1
      else
        break
      end
    end
    if count == 0 then error end
    result

  fun ref _parse_string(is_key: Bool) ? =>
    _eat('"')?
    _token_start = _offset
    var buf = recover String end
    while true do
      match \exhaustive\ _next()?
      | '"' => break
      | '\\' =>
        match _next()?
        | '"'  => buf.push('"')
        | '\\' => buf.push('\\')
        | '/'  => buf.push('/')
        | 'b'  => buf.push(0x08)
        | 'f'  => buf.push(0x0C)
        | 'n'  => buf.push('\n')
        | 'r'  => buf.push('\r')
        | 't'  => buf.push('\t')
        | 'u'  => buf = _parse_unicode(consume buf)?
        else error
        end
      | let c: U8 if c < 0x20 => error
      | let c: U8 => buf.push(c)
      end
    end
    last_string = consume buf
    _emit(if is_key then JsonTokenKey else JsonTokenString end)?

  fun ref _parse_unicode(buf: String iso): String iso^ ? =>
    let value = _read_hex4()?

    if (value >= 0xD800) and (value < 0xDC00) then
      // High surrogate — expect \uXXXX low surrogate
      _eat('\\')?; _eat('u')?
      let low = _read_hex4()?
      if (low >= 0xDC00) and (low < 0xE000) then
        let combined =
          0x10000 + (((value and 0x3FF) << 10) or (low and 0x3FF))
        buf.append(recover val String.from_utf32(combined) end)
      else
        error
      end
    elseif (value >= 0xDC00) and (value < 0xE000) then
      error // lone low surrogate
    else
      buf.append(recover val String.from_utf32(value) end)
    end
    consume buf

  fun ref _read_hex4(): U32 ? =>
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
        error
      end
      result = (result << 4) or digit
      i = i + 1
    end
    result

  // --- Character primitives ---

  fun _peek(): U8 ? =>
    _source(_offset)?

  fun _peek_safe(): U8 =>
    try _source(_offset)? else 0 end

  fun ref _next(): U8 ? =>
    let c = _source(_offset)?
    _offset = _offset + 1
    if c == '\n' then _line = _line + 1 end
    c

  fun ref _eat(expected: U8) ? =>
    if _source(_offset)? != expected then error end
    _offset = _offset + 1
    if expected == '\n' then _line = _line + 1 end

  fun ref _skip_whitespace() =>
    while _offset < _source.size() do
      match try _source(_offset)? else return end
      | ' ' | '\t' | '\r' => _offset = _offset + 1
      | '\n' => _offset = _offset + 1; _line = _line + 1
      else return
      end
    end
