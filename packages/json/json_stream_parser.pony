use "buffered"

primitive JsonNeedMore
  """
  Returned by `JsonStreamParser.pull()` when no complete top-level value is
  available yet: the bytes fed so far are valid JSON but incomplete. Feed more
  bytes with `feed()` and call `pull()` again. Expected at every chunk boundary
  that falls inside a value.
  """

type JsonStreamResult is (JsonValue | JsonNeedMore | JsonParseError)
  """
  The outcome of a `JsonStreamParser.pull()`: a completed top-level `JsonValue`,
  `JsonNeedMore` (feed more bytes), or a `JsonParseError` (the input is
  malformed; the parser latches into the error state).
  """

class ref JsonStreamParser
  """
  Incremental, pull-based JSON parser.

  Feed bytes with `feed()` as they arrive, then call `pull()` to take complete
  top-level values one at a time. `pull()` returns the same `JsonValue` the
  batch `JsonParser` returns, so a streamed value flows into `JsonNav`,
  `JsonLens`, `JsonPath`, and `JsonPrinter` unchanged.

  ```pony
  let parser = JsonStreamParser
  parser.feed(chunk)
  // Drain every value the fed bytes completed; one chunk may yield zero, one,
  // or several values, so loop until pull() returns JsonNeedMore.
  var draining = true
  while draining do
    match parser.pull()
    | let v: JsonValue      => // a complete top-level value
    | JsonNeedMore          => draining = false // feed more bytes, pull() again
    | let e: JsonParseError => draining = false // malformed; parser now latched
    end
  end
  ```

  It parses a *stream* of top-level values, each of which must be an object or
  array: a single document yields one value; a sequence of object/array values
  (newline-delimited JSON whose lines are objects or arrays, concatenated values,
  a socket delivering messages) yields each value as it completes, with no
  per-document setup. A bare scalar top-level value (`42`, `"hi"`) is not
  supported — a top-level value must be a container, because its closing `}`/`]`
  is what signals completion. This differs from `JsonParser`, which accepts a
  scalar document. (A bare-scalar element in a stream — a `42` line in otherwise
  object-per-line NDJSON — is rejected and latches the parser; see below.)

  Completion is structural: a value is done when its root container closes. There
  is intentionally no end-of-stream call. If a producer feeds an incomplete value
  and then stops, `pull()` simply keeps returning `JsonNeedMore` — noticing a
  truncated feed is the consumer's concern, not the parser's.

  Malformed input latches: once `pull()` returns a `JsonParseError`, the stream
  is considered corrupt and every later `pull()` returns that same error. Recover
  by constructing a new parser.
  """
  embed _reader: Reader
  embed _assembler: _TreeAssembler
  embed _frames: Array[_StreamFrame]
  var _scan: (_LeafScan | None)
  let _limits: JsonStreamLimits val
  var _error: (JsonParseError | None)
  var _value_bytes: USize
  var _line: USize
  var _offset: USize

  new ref create(limits: JsonStreamLimits val = JsonStreamLimits) =>
    _reader = Reader
    _assembler = _TreeAssembler
    _frames = Array[_StreamFrame]
    _scan = None
    _limits = limits
    _error = None
    _value_bytes = 0
    _line = 1
    _offset = 0

  fun ref feed(data: ByteSeq) =>
    """
    Append a chunk of bytes to be parsed. `ByteSeq` is `(String | Array[U8]
    val)`; an `Array[U8] iso` from a socket moves to `val` with no copy via
    `consume`. After the parser has latched an error, `feed()` is a no-op.
    """
    // Drop empty chunks: a producer that loops feeding zero-byte reads would
    // otherwise accumulate buffer nodes that never drain.
    if (_error is None) and (data.size() > 0) then
      _reader.append(data)
    end

  fun ref pull(): JsonStreamResult =>
    """
    Take the next complete top-level value, or `JsonNeedMore` if more bytes are
    needed, or a latched `JsonParseError`. Call `pull()` repeatedly until it
    returns `JsonNeedMore`: a single fed chunk can complete several values (or
    none), so a one-pull-per-feed loop would silently drop values.
    """
    match _error
    | let e: JsonParseError => return e
    end

    var result: JsonStreamResult = JsonNeedMore
    var looping = true
    while looping do
      match _scan
      | let ss: _StringScan =>
        match _resume_string(ss)
        | _ScanNeedMore => result = JsonNeedMore; looping = false
        | let e: JsonParseError => result = _fail(e); looping = false
        | _ScanComplete =>
          // Move the decoded buffer out as an immutable value (no copy),
          // matching the batch parser's `consume buf`.
          let decoded: String iso = ss.buf = recover iso String end
          if ss.is_key then
            _assembler.key(consume decoded)
            _set_object_pos(_ObjColon)
          else
            _assembler.value(consume decoded)
          end
          _scan = None
        end
      | let ns: _NumberScan =>
        match _resume_number(ns)
        | _ScanNeedMore => result = JsonNeedMore; looping = false
        | let e: JsonParseError => result = _fail(e); looping = false
        | _ScanComplete =>
          match _NumberParse(ns.buf)
          | let v: I64 => _assembler.value(v); _scan = None
          | let v: F64 => _assembler.value(v); _scan = None
          | _NumberMalformed =>
            result = _fail(_err("invalid number")); looping = false
          end
        end
      | let ks: _KeywordScan =>
        match _resume_keyword(ks)
        | _ScanNeedMore => result = JsonNeedMore; looping = false
        | let e: JsonParseError => result = _fail(e); looping = false
        | _ScanComplete => _assembler.value(ks.value); _scan = None
        end
      | None =>
        match _dispatch()
        | _StepGo => None
        | let r: JsonStreamResult => result = r; looping = false
        end
      end
    end
    result

  // --- dispatch between tokens --------------------------------------------

  fun ref _dispatch(): (_StepGo | JsonStreamResult) =>
    _skip_whitespace()
    // Whitespace before/between top-level values belongs to no value, so it
    // does not count against the next value's max_value_bytes budget.
    if _frames.size() == 0 then _value_bytes = 0 end
    let b = try _reader.peek_u8()? else return JsonNeedMore end
    if _value_bytes > _limits.max_value_bytes then
      return _fail(_err("value exceeds maximum size"))
    end
    if _frames.size() == 0 then
      _dispatch_root(b)
    else
      match _top_frame()
      | let f: _ArrayFrame => _dispatch_array(f, b)
      | let f: _ObjectFrame => _dispatch_object(f, b)
      end
    end

  fun ref _top_frame(): _StreamFrame =>
    // Only called when _frames is non-empty.
    try _frames(_frames.size() - 1)?
    else _Unreachable(); _ArrayFrame(_ArrValueOrEnd)
    end

  fun ref _dispatch_root(b: U8): (_StepGo | JsonStreamResult) =>
    match b
    | '{' => _open_object()
    | '[' => _open_array()
    else
      _fail(_err("expected an object or array at the top level"))
    end

  fun ref _dispatch_array(f: _ArrayFrame, b: U8): (_StepGo | JsonStreamResult) =>
    match f.pos
    | _ArrValueOrEnd =>
      if b == ']' then _close_array()
      else f.pos = _ArrCommaOrEnd; _begin_value(b) end
    | _ArrValue =>
      if b == ']' then _fail(_err("trailing comma in array"))
      else f.pos = _ArrCommaOrEnd; _begin_value(b) end
    | _ArrCommaOrEnd =>
      if b == ',' then _consume(); f.pos = _ArrValue; _StepGo
      elseif b == ']' then _close_array()
      else _fail(_err("expected ',' or ']' in array")) end
    end

  fun ref _dispatch_object(f: _ObjectFrame, b: U8): (_StepGo | JsonStreamResult)
  =>
    match f.pos
    | _ObjKeyOrEnd =>
      if b == '}' then _close_object()
      elseif b == '"' then _begin_key()
      else _fail(_err("expected a key or '}' in object")) end
    | _ObjKey =>
      if b == '"' then _begin_key()
      else _fail(_err("expected a key in object")) end
    | _ObjColon =>
      if b == ':' then _consume(); f.pos = _ObjValue; _StepGo
      else _fail(_err("expected ':' in object")) end
    | _ObjValue =>
      f.pos = _ObjCommaOrEnd; _begin_value(b)
    | _ObjCommaOrEnd =>
      if b == ',' then _consume(); f.pos = _ObjKey; _StepGo
      elseif b == '}' then _close_object()
      else _fail(_err("expected ',' or '}' in object")) end
    end

  fun ref _begin_value(b: U8): (_StepGo | JsonStreamResult) =>
    match b
    | '{' => _open_object()
    | '[' => _open_array()
    | '"' => _consume(); _scan = _StringScan(false); _StepGo
    | 't' => _scan = _KeywordScan("true", true); _StepGo
    | 'f' => _scan = _KeywordScan("false", false); _StepGo
    | 'n' => _scan = _KeywordScan("null", None); _StepGo
    else
      if (b == '-') or ((b >= '0') and (b <= '9')) then
        _scan = _NumberScan
        _StepGo
      else
        _fail(_err("expected a value"))
      end
    end

  fun ref _begin_key(): (_StepGo | JsonStreamResult) =>
    _consume() // opening '"'
    _scan = _StringScan(true)
    _StepGo

  fun ref _open_object(): (_StepGo | JsonStreamResult) =>
    if _frames.size() >= _limits.max_depth then
      return _fail(_err("maximum nesting depth exceeded"))
    end
    _consume() // '{'
    _assembler.begin_object()
    _frames.push(_ObjectFrame(_ObjKeyOrEnd))
    _StepGo

  fun ref _open_array(): (_StepGo | JsonStreamResult) =>
    if _frames.size() >= _limits.max_depth then
      return _fail(_err("maximum nesting depth exceeded"))
    end
    _consume() // '['
    _assembler.begin_array()
    _frames.push(_ArrayFrame(_ArrValueOrEnd))
    _StepGo

  fun ref _close_object(): (_StepGo | JsonStreamResult) =>
    _consume() // '}'
    _assembler.end_object()
    try _frames.pop()? else _Unreachable() end
    _maybe_complete()

  fun ref _close_array(): (_StepGo | JsonStreamResult) =>
    _consume() // ']'
    _assembler.end_array()
    try _frames.pop()? else _Unreachable() end
    _maybe_complete()

  fun ref _maybe_complete(): (_StepGo | JsonStreamResult) =>
    if _frames.size() == 0 then
      // top-level value complete; hand it back and reset for the next one
      match _assembler.result()
      | let v: JsonValue =>
        // _value_bytes is reset by _dispatch when it next sees the root frame,
        // so it is not reset here (single owner).
        _assembler.reset()
        v
      | _NoResult => _Unreachable(); JsonNeedMore
      end
    else
      _StepGo
    end

  fun ref _set_object_pos(pos: _ObjPos) =>
    match _top_frame()
    | let f: _ObjectFrame => f.pos = pos
    else _Unreachable()
    end

  // --- leaf scanners (suspendable) ----------------------------------------

  fun ref _resume_string(s: _StringScan): _ScanOutcome =>
    while _reader.size() > 0 do
      if s.buf.size() > _limits.max_string_len then
        return _err("string exceeds maximum length")
      end
      if _value_bytes > _limits.max_value_bytes then
        return _err("value exceeds maximum size")
      end
      let c = _take()
      match s.phase
      | _StrNormal =>
        if (s.pending_high isnt None) and (c != '\\') then
          return _err("unpaired high surrogate")
        end
        if c == '"' then
          return _ScanComplete
        elseif c == '\\' then
          s.phase = _StrEscape
        elseif c < 0x20 then
          return _err("control character in string")
        else
          s.buf.push(c)
        end
      | _StrEscape =>
        if (s.pending_high isnt None) and (c != 'u') then
          return _err("unpaired high surrogate")
        end
        match c
        | '"' => s.buf.push('"'); s.phase = _StrNormal
        | '\\' => s.buf.push('\\'); s.phase = _StrNormal
        | '/' => s.buf.push('/'); s.phase = _StrNormal
        | 'b' => s.buf.push(0x08); s.phase = _StrNormal
        | 'f' => s.buf.push(0x0C); s.phase = _StrNormal
        | 'n' => s.buf.push('\n'); s.phase = _StrNormal
        | 'r' => s.buf.push('\r'); s.phase = _StrNormal
        | 't' => s.buf.push('\t'); s.phase = _StrNormal
        | 'u' => s.phase = _StrUnicode; s.hex_value = 0; s.hex_count = 0
        else return _err("invalid escape sequence")
        end
      | _StrUnicode =>
        match _hex_digit(c)
        | let hd: U32 =>
          s.hex_value = (s.hex_value << 4) or hd
          s.hex_count = s.hex_count + 1
          if s.hex_count == 4 then
            match _apply_unicode(s)
            | let e: JsonParseError => return e
            end
          end
        | None => return _err("invalid unicode escape")
        end
      end
    end
    _ScanNeedMore

  fun ref _apply_unicode(s: _StringScan): (None | JsonParseError) =>
    let value = s.hex_value
    match s.pending_high
    | let high: U32 =>
      if (value >= 0xDC00) and (value < 0xE000) then
        let combined: U32 =
          0x10000 + (((high and 0x3FF) << 10) or (value and 0x3FF))
        s.buf.append(recover val String.from_utf32(combined) end)
        s.pending_high = None
        s.phase = _StrNormal
        None
      else
        _err("expected a low surrogate")
      end
    | None =>
      if (value >= 0xD800) and (value < 0xDC00) then
        // High surrogate: a low-surrogate `\u` escape must follow immediately.
        // That invariant is enforced by the `pending_high isnt None` guards in
        // _resume_string's _StrNormal/_StrEscape arms (which reject any byte
        // other than `\` then `u` while a high surrogate is pending).
        s.pending_high = value
        s.phase = _StrNormal
        None
      elseif (value >= 0xDC00) and (value < 0xE000) then
        _err("unexpected low surrogate")
      else
        s.buf.append(recover val String.from_utf32(value) end)
        s.phase = _StrNormal
        None
      end
    end

  fun ref _resume_number(s: _NumberScan): _ScanOutcome =>
    while _reader.size() > 0 do
      let c = try _reader.peek_u8()? else return _ScanNeedMore end
      if not _is_number_byte(c) then
        // a following non-number byte terminates the number; leave it in the
        // reader for dispatch. A number is never completed on exhaustion.
        return _ScanComplete
      end
      // `>=` (vs the string scanner's `>`) keeps the bound tight: a number's
      // terminator is peeked, not consumed in the loop, so we must reject the
      // byte that *would* exceed the limit before pushing it.
      if s.buf.size() >= _limits.max_number_len then
        return _err("number exceeds maximum length")
      end
      if _value_bytes > _limits.max_value_bytes then
        return _err("value exceeds maximum size")
      end
      _consume()
      s.buf.push(c)
    end
    _ScanNeedMore

  fun ref _resume_keyword(s: _KeywordScan): _ScanOutcome =>
    // No length limit is needed: a keyword is at most 5 bytes (`false`), and
    // max_value_bytes still bounds the enclosing value.
    while s.matched < s.word.size() do
      if _reader.size() == 0 then return _ScanNeedMore end
      let c = _take()
      let expected = try s.word(s.matched)? else _Unreachable(); U8(0) end
      if c != expected then return _err("invalid literal") end
      s.matched = s.matched + 1
    end
    _ScanComplete

  // --- byte primitives ----------------------------------------------------

  fun ref _skip_whitespace() =>
    while _reader.size() > 0 do
      match try _reader.peek_u8()? else return end
      | ' ' | '\t' | '\r' | '\n' => _consume()
      else return
      end
    end

  fun ref _consume() =>
    try
      let c = _reader.u8()?
      _offset = _offset + 1
      _value_bytes = _value_bytes + 1
      if c == '\n' then _line = _line + 1 end
    else
      _Unreachable()
    end

  fun ref _take(): U8 =>
    try
      let c = _reader.u8()?
      _offset = _offset + 1
      _value_bytes = _value_bytes + 1
      if c == '\n' then _line = _line + 1 end
      c
    else
      _Unreachable()
      U8(0)
    end

  fun _hex_digit(c: U8): (U32 | None) =>
    if (c >= '0') and (c <= '9') then (c - '0').u32()
    elseif (c >= 'a') and (c <= 'f') then ((c - 'a') + 10).u32()
    elseif (c >= 'A') and (c <= 'F') then ((c - 'A') + 10).u32()
    else None
    end

  fun _is_number_byte(c: U8): Bool =>
    ((c >= '0') and (c <= '9')) or (c == '-') or (c == '+') or (c == '.')
      or (c == 'e') or (c == 'E')

  fun _err(message: String): JsonParseError =>
    JsonParseError(message, _offset, _line)

  fun ref _fail(e: JsonParseError): JsonStreamResult =>
    _error = e
    e
