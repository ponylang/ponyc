class ref JsonTokenParser
  """
  Streaming, incremental JSON token parser.

  Feed bytes as they arrive with `feed()`; the parser walks the JSON structure to
  any depth and pushes tokens to a `JsonTokenNotify` callback as they complete. It
  builds no tree — its working memory is the container-depth stack, the single
  string or number it is part-way through, and the fed bytes it has not yet
  consumed. A value split across a chunk boundary is held and stitched onto the
  next `feed()`, so you never manage leftover bytes.

  ```pony
  let parser = JsonTokenParser(
    object is JsonTokenNotify
      fun ref apply(p: JsonTokenParser, token: JsonToken) =>
        match token
        | let k: JsonTokenKey    => // k.value
        | let s: JsonTokenString => // s.value
        | let n: JsonTokenNumber => // n.value
        | JsonTokenObjectStart   => // ...
        end
    end)
  parser.feed(chunk)?
  ```

  It parses a *stream* of top-level values: after one value's tokens end it
  continues to the next, so newline-delimited JSON or a socket delivering messages
  back to back works with no per-value setup. Feeding a whole document at once is
  just the case where every byte is already in hand.

  A number is the one value with no self-delimiter — the parser cannot know a
  number is finished until a following non-number byte arrives, so a number at the
  very end of the fed bytes is not emitted until more bytes come or you call
  `finish()`. `finish()` says "no more bytes are coming" and completes it.

  Malformed input raises from `feed()`/`finish()`; read `describe_error()`,
  `token_end()`, and `line()` for the location. After a raise the parser latches:
  every later `feed()`/`finish()` raises again. `abort()` (called from the
  notifier) stops the parse the same way. For untrusted input, `JsonParseLimits`
  caps nesting depth and the length of a single string or number.

  To turn a token stream back into a `JsonValue`, use `JsonReassembler`. For a
  whole in-memory document, `JsonParser.parse()` is simpler.
  """

  let _notify: JsonTokenNotify
  let _limits: JsonParseLimits
  embed _reader: _ChunkReader
  embed _frames: Array[_StreamFrame]
  var _scan: (_LeafScan | None) = None
  var _offset: USize = 0
  var _line: USize = 1
  var _token_start: USize = 0
  var _state: _ParserState = _Open
  var _error_message: String = ""
  var _running: Bool = false

  new ref create(
    notify: JsonTokenNotify,
    limits: JsonParseLimits = JsonParseLimits)
  =>
    _notify = notify
    _limits = limits
    _reader = _ChunkReader
    _frames = Array[_StreamFrame]

  fun ref feed(data: ByteSeq) ? =>
    """
    Append a chunk of bytes and emit every token now derivable from the bytes in
    hand. Holds any partial value to be finished by a later `feed()`. Raises on
    malformed input or `abort()`; after that the parser is done and every later
    `feed()`/`finish()` raises. Do not call `feed()` or `finish()` from within the
    notifier — the parse is running and a re-entrant call raises.
    """
    if (_state isnt _Open) or _running then error end
    // Drop empty chunks: a producer that loops feeding zero-byte reads would
    // otherwise pile up empty buffer nodes that never drain.
    if data.size() > 0 then _reader.append(data) end
    _running = true
    try _run()? else _running = false; error end
    _running = false

  fun ref finish() ? =>
    """
    Signal that no more bytes will be fed. Completes a pending number (the one
    value that otherwise waits for a following byte) and emits its token. Raises if
    that number's text is not valid, or if aborted/already done. A value left
    structurally incomplete (an open container, an unterminated string) is simply
    not completed — the consumer sees the truncation via `incomplete()`. After
    `finish()` the parser is done; feeding more raises.
    """
    if (_state isnt _Open) or _running then error end
    _running = true
    try
      match _scan
      | let ns: _NumberScan =>
        _token_start = ns.start
        _scan = None
        match _NumberParse(ns.buf)
        | let v: (I64 | F64) => _emit(JsonTokenNumber(v))?
        | _NumberMalformed => _fail_msg("invalid number")?
        end
      end
      _running = false
    else
      _running = false
      error
    end
    _state = _Finished

  fun ref abort() =>
    """
    Signal the parser to stop. The current `feed()`/`finish()` then raises, and the
    parser is done. Call this from the notifier when you have seen enough.
    """
    _state = _Aborted

  fun token_start(): USize =>
    """
    Byte offset where the current token starts, absolute across all feeds. Valid
    only during the notify callback.
    """
    _token_start

  fun token_end(): USize =>
    """
    Byte offset just past the current token, absolute across all feeds. Valid only
    during the notify callback.
    """
    _offset

  fun line(): USize =>
    """Current line number (1-based)."""
    _line

  fun incomplete(): Bool =>
    """
    True while a value is part-way through — either mid-scan on a leaf or with a
    container still open. After feeding a complete document and calling `finish()`
    this is false; if it is still true, the input ended in the middle of a value.
    This is the byte-level truncation check; `JsonReassembler.mid_value()` reports
    only whether the reassembler is holding a partial value's tokens.
    """
    (_scan isnt None) or (_frames.size() > 0)

  fun describe_error(): String =>
    """Human-readable description of the most recent error."""
    match _state
    | _Aborted => "Parse aborted by the notifier"
    | _Finished => "Input already finished"
    else
      if _error_message != "" then
        _error_message
      else
        "Invalid JSON at byte offset " + _offset.string()
          + ", line " + _line.string()
      end
    end

  fun parse_error(): JsonParseError =>
    """
    The most recent error as a `JsonParseError` (message, byte offset, line).
    Meaningful after a raise from `feed()`/`finish()`.
    """
    JsonParseError(describe_error(), _offset, _line)

  // --- driving loop -------------------------------------------------------

  fun ref _run() ? =>
    var progress = true
    while progress do
      match _scan
      | let ss: _StringScan =>
        match _resume_string(ss)
        | _ScanNeedMore => progress = false
        | let e: JsonParseError => _fail(e)?
        | _ScanComplete =>
          let value: String = try ss.result as String val
            else _Unreachable(); "" end
          _token_start = ss.start
          _scan = None
          if ss.is_key then
            _emit(JsonTokenKey(value))?
            _set_object_pos(_ObjColon)
          else
            _emit(JsonTokenString(value))?
          end
        end
      | let ns: _NumberScan =>
        match _resume_number(ns)
        | _ScanNeedMore => progress = false
        | let e: JsonParseError => _fail(e)?
        | _ScanComplete =>
          _token_start = ns.start
          _scan = None
          match _NumberParse(ns.buf)
          | let v: (I64 | F64) => _emit(JsonTokenNumber(v))?
          | _NumberMalformed => _fail_msg("invalid number")?
          end
        end
      | let ks: _KeywordScan =>
        match _resume_keyword(ks)
        | _ScanNeedMore => progress = false
        | let e: JsonParseError => _fail(e)?
        | _ScanComplete =>
          _token_start = ks.start
          let t = ks.token
          _scan = None
          _emit(t)?
        end
      | None =>
        match _dispatch()?
        | _StepGo => None
        | _ScanNeedMore => progress = false
        end
      end
    end

  // --- dispatch between tokens --------------------------------------------

  fun ref _dispatch(): (_StepGo | _ScanNeedMore) ? =>
    _skip_whitespace()
    let b = try _reader.peek()? else return _ScanNeedMore end
    if _frames.size() == 0 then
      _begin_value(b)?
    else
      match _top_frame()
      | let f: _ArrayFrame => _dispatch_array(f, b)?
      | let f: _ObjectFrame => _dispatch_object(f, b)?
      end
    end

  fun ref _top_frame(): _StreamFrame =>
    try _frames(_frames.size() - 1)?
    else _Unreachable(); _ArrayFrame(_ArrValueOrEnd)
    end

  fun ref _dispatch_array(f: _ArrayFrame, b: U8): _StepGo ? =>
    match f.pos
    | _ArrValueOrEnd =>
      if b == ']' then _close_array()?
      else f.pos = _ArrCommaOrEnd; _begin_value(b)? end
    | _ArrValue =>
      if b == ']' then _fail_msg("trailing comma in array")?
      else f.pos = _ArrCommaOrEnd; _begin_value(b)? end
    | _ArrCommaOrEnd =>
      if b == ',' then _consume(); f.pos = _ArrValue; _StepGo
      elseif b == ']' then _close_array()?
      else _fail_msg("expected ',' or ']' in array")? end
    end

  fun ref _dispatch_object(f: _ObjectFrame, b: U8): _StepGo ? =>
    match f.pos
    | _ObjKeyOrEnd =>
      if b == '}' then _close_object()?
      elseif b == '"' then _begin_key()
      else _fail_msg("expected a key or '}' in object")? end
    | _ObjKey =>
      if b == '"' then _begin_key()
      else _fail_msg("expected a key in object")? end
    | _ObjColon =>
      if b == ':' then _consume(); f.pos = _ObjValue; _StepGo
      else _fail_msg("expected ':' in object")? end
    | _ObjValue =>
      f.pos = _ObjCommaOrEnd; _begin_value(b)?
    | _ObjCommaOrEnd =>
      if b == ',' then _consume(); f.pos = _ObjKey; _StepGo
      elseif b == '}' then _close_object()?
      else _fail_msg("expected ',' or '}' in object")? end
    end

  fun ref _begin_value(b: U8): _StepGo ? =>
    match b
    | '{' => _open_object()?
    | '[' => _open_array()?
    | '"' =>
      let start = _offset
      _consume() // opening '"'
      _reader.scan_reset() // scan cursor starts at the string content
      _scan = _StringScan(false, start)
      _StepGo
    | 't' => _scan = _KeywordScan(_offset, "true", JsonTokenTrue); _StepGo
    | 'f' => _scan = _KeywordScan(_offset, "false", JsonTokenFalse); _StepGo
    | 'n' => _scan = _KeywordScan(_offset, "null", JsonTokenNull); _StepGo
    else
      if (b == '-') or ((b >= '0') and (b <= '9')) then
        _scan = _NumberScan(_offset)
        _StepGo
      else
        _fail_msg("expected a value")?
      end
    end

  fun ref _begin_key(): _StepGo =>
    let start = _offset
    _consume() // opening '"'
    _reader.scan_reset() // scan cursor starts at the string content
    _scan = _StringScan(true, start)
    _StepGo

  fun ref _open_object(): _StepGo ? =>
    if _frames.size() >= _limits.max_depth then
      _fail_msg("maximum nesting depth exceeded")?
    end
    _token_start = _offset
    _consume() // '{'
    _emit(JsonTokenObjectStart)?
    _frames.push(_ObjectFrame(_ObjKeyOrEnd))
    _StepGo

  fun ref _open_array(): _StepGo ? =>
    if _frames.size() >= _limits.max_depth then
      _fail_msg("maximum nesting depth exceeded")?
    end
    _token_start = _offset
    _consume() // '['
    _emit(JsonTokenArrayStart)?
    _frames.push(_ArrayFrame(_ArrValueOrEnd))
    _StepGo

  fun ref _close_object(): _StepGo ? =>
    _token_start = _offset
    _consume() // '}'
    _emit(JsonTokenObjectEnd)?
    try _frames.pop()? else _Unreachable() end
    _StepGo

  fun ref _close_array(): _StepGo ? =>
    _token_start = _offset
    _consume() // ']'
    _emit(JsonTokenArrayEnd)?
    try _frames.pop()? else _Unreachable() end
    _StepGo

  fun ref _set_object_pos(pos: _ObjPos) =>
    match _top_frame()
    | let f: _ObjectFrame => f.pos = pos
    else _Unreachable()
    end

  // --- leaf scanners (suspendable) ----------------------------------------

  fun ref _resume_string(s: _StringScan): _ScanOutcome =>
    // Scan forward for the closing quote WITHOUT consuming, tracking whether any
    // escape appears. A string with no escapes that lies within one chunk can
    // then be handed back as a zero-copy view; anything else is decoded. The scan
    // resumes across feeds from where it stopped, so it visits each byte once.
    while not _reader.scan_at_end() do
      if _reader.scan_len() > _limits.max_string_len then
        return _err("string exceeds maximum length")
      end
      let c = try _reader.scan_byte()? else return _ScanNeedMore end
      if s.escaped then
        s.escaped = false
        _reader.scan_advance()
      elseif c == '"' then
        return _finish_string(s)
      elseif c == '\\' then
        s.had_escape = true
        s.escaped = true
        _reader.scan_advance()
      elseif c < 0x20 then
        return _err("control character in string")
      else
        _reader.scan_advance()
      end
    end
    _ScanNeedMore

  fun ref _finish_string(s: _StringScan): _ScanOutcome =>
    // The scan cursor is at the closing quote; the content is [consume, scan).
    if (not s.had_escape) and _reader.scan_single_chunk() then
      match _reader.view()
      | let v: String val =>
        let n = _reader.scan_len()
        _reader.skip(n + 1) // content + closing quote
        // The content holds no raw newline (control bytes are rejected above), so
        // only the byte offset moves; the line number is unchanged.
        _offset = _offset + n + 1
        s.result = v
        return _ScanComplete
      end
    end
    // Copy path: escapes, or the content spans chunks — decode it. The whole
    // string is present now, so the decode runs in one pass and cannot suspend,
    // and it re-checks no limit: the extent scan already bounded the source
    // length, and every JSON escape decodes to no more bytes than it spans
    // (`\n` 2->1, `\uXXXX` 6->at most 3), so the decoded buffer is bounded too.
    // A new escape whose decoded form could exceed its source would break that.
    _decode_string(s)

  fun ref _decode_string(s: _StringScan): _ScanOutcome =>
    while _reader.size() > 0 do
      let c = _take()
      match s.phase
      | _StrNormal =>
        // A pending high surrogate must be followed immediately by `\u`; this
        // guard (and its twin in _StrEscape) is what lets _apply_unicode assume
        // a low surrogate follows. Don't drop it.
        if (s.pending_high isnt None) and (c != '\\') then
          return _err("unpaired high surrogate")
        end
        if c == '"' then
          s.result = s.buf = recover iso String end
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
    // The extent scan already found the closing quote, so we never run dry here.
    _Unreachable()
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
        // The `pending_high isnt None` guards in _resume_string reject any byte
        // other than `\` then `u` while a high surrogate is pending.
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
      // The loop guard guarantees a byte, so the peek cannot fail here.
      let c = try _reader.peek()? else _Unreachable(); U8(0) end
      if not _is_number_byte(c) then
        // A following non-number byte terminates the number; leave it in the
        // reader for dispatch. A number is never completed on exhaustion.
        return _ScanComplete
      end
      // `>=` keeps the bound tight: the terminator is peeked, not consumed here,
      // so reject the byte that would exceed the limit before pushing it.
      if s.buf.size() >= _limits.max_number_len then
        return _err("number exceeds maximum length")
      end
      _consume()
      s.buf.push(c)
    end
    _ScanNeedMore

  fun ref _resume_keyword(s: _KeywordScan): _ScanOutcome =>
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
      match try _reader.peek()? else return end
      | ' ' | '\t' | '\r' | '\n' => _consume()
      else return
      end
    end

  fun ref _consume() =>
    try
      let c = _reader.peek()?
      _reader.skip1()
      _offset = _offset + 1
      if c == '\n' then _line = _line + 1 end
    else
      _Unreachable()
    end

  fun ref _take(): U8 =>
    try
      let c = _reader.peek()?
      _reader.skip1()
      _offset = _offset + 1
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

  // --- token emission and failure -----------------------------------------

  fun ref _emit(token: JsonToken) ? =>
    _notify(this, token)
    if _state is _Aborted then error end

  fun _err(message: String): JsonParseError =>
    JsonParseError(message, _offset, _line)

  fun ref _set_error(message: String) =>
    _state = _Failed
    _error_message = message

  fun ref _fail(e: JsonParseError) ? =>
    _set_error(e.message)
    error

  fun ref _fail_msg(message: String): _StepGo ? =>
    // Always raises; the `_StepGo` return type is only so a dispatch branch that
    // fails can stand where a `_StepGo` value is expected.
    _set_error(message)
    error
