use pc = "collections/persistent"

class ref _ObjectInProgress
  var map: pc.Map[String, JSONValue]
  var pending_key: (String | None)

  new ref create() =>
    map = pc.Map[String, JSONValue]
    pending_key = None

class ref _ArrayInProgress
  var vec: pc.Vec[JSONValue]

  new ref create() =>
    vec = pc.Vec[JSONValue]

class ref JSONReassembler is JSONTokenNotify
  """
  Assembles a run of JSON tokens into `JSONValue`s.

  This is the opt-in path from a token stream back to a materialized value. Plug
  it into a `JSONTokenParser` — it is a `JSONTokenNotify`, so tokens flow
  into it directly — or hand it tokens yourself with `add()`. Either way,
  `take_values()` returns the top-level values that have completed, the same
  `JSONValue` the batch `JSONParser` would build:

  ```pony
  let re = JSONReassembler
  let parser = JSONTokenParser(re)
  parser.feed(chunk)?
  for value in re.take_values().values() do
    // value : JSONValue — use with JSONNav, JSONLens, JSONPath, JSONPrinter
  end
  ```

  Memory is the caller's choice: building a value costs memory proportional
  to that value, so hand the reassembler only the tokens you actually want
  materialized and drop the rest. There is no built-in size cap on what it
  builds, so do not blind-drain an untrusted stream into it.

  `take_values()` returns only top-level values that have completed, and
  drains them — it is not a plain accessor. If a run ends part-way through
  a value, that value stays buffered — `mid_value()` reports it — and the
  tokens that finish it can arrive later. `add()` expects tokens in the
  order `JSONTokenParser` emits them; a run that is not a valid emission
  sequence produces an unspecified result.
  """
  embed _stack: Array[(_ObjectInProgress | _ArrayInProgress)]
  var _completed: Array[JSONValue] iso

  new ref create() =>
    _stack = Array[(_ObjectInProgress | _ArrayInProgress)]
    _completed = recover iso Array[JSONValue] end

  fun ref add(token: JSONToken) =>
    """
    Fold one token into the value under construction. Tokens must arrive in the
    order `JSONTokenParser` emits them; a run that is not a valid emission
    sequence produces an unspecified result (but never corrupts a well-formed
    run that follows a `take_values()`).
    """
    match \exhaustive\ token
    | JSONTokenObjectStart => _stack.push(_ObjectInProgress)
    | JSONTokenArrayStart => _stack.push(_ArrayInProgress)
    | let k: JSONTokenKey =>
      try
        match _stack(_stack.size() - 1)?
        | let obj: _ObjectInProgress => obj.pending_key = k.value
        end
      end
    | let s: JSONTokenString => _add_value(s.value)
    | let n: JSONTokenNumber => _add_value(n.value)
    | JSONTokenTrue => _add_value(true)
    | JSONTokenFalse => _add_value(false)
    | JSONTokenNull => _add_value(None)
    | JSONTokenObjectEnd =>
      // Check the top's type before popping, so a mismatched closer on an
      // invalid run leaves the stack intact rather than dropping the container
      // it popped.
      try
        match _stack(_stack.size() - 1)?
        | let obj: _ObjectInProgress =>
          _stack.pop()?
          _add_value(JSONObject(obj.map))
        end
      end
    | JSONTokenArrayEnd =>
      try
        match _stack(_stack.size() - 1)?
        | let arr: _ArrayInProgress =>
          _stack.pop()?
          _add_value(JSONArray(arr.vec))
        end
      end
    end

  fun ref apply(parser: JSONTokenParser, token: JSONToken) =>
    """
    `JSONTokenNotify` entry point — folds the token in, ignoring `parser`.
    """
    add(token)

  fun ref take_values(): Array[JSONValue] iso^ =>
    """
    Take the top-level values completed since the last call, and reset — a
    second call returns an empty array unless more values have completed. A
    value whose root container has not yet closed is not included; check
    `mid_value()` for it.
    """
    _completed = recover iso Array[JSONValue] end

  fun mid_value(): Bool =>
    """
    True while tokens have been folded for a value that has not closed yet.
    This reports only the reassembler's own buffered partial; for byte-level
    truncation of a stream, use `JSONTokenParser.incomplete()`.
    """
    _stack.size() > 0

  fun ref _add_value(v: JSONValue) =>
    if _stack.size() == 0 then
      _completed.push(v)
    else
      try
        match \exhaustive\ _stack(_stack.size() - 1)?
        | let obj: _ObjectInProgress =>
          match obj.pending_key
          | let key: String =>
            obj.map = obj.map(key) = v
            obj.pending_key = None
          end
        | let arr: _ArrayInProgress =>
          arr.vec = arr.vec.push(v)
        end
      end
    end
