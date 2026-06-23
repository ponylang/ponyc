class ref _TreeBuilder is JsonTokenNotify
  """
  Internal token consumer that assembles a JsonValue tree from token events.
  Used by JsonParser to build the full parse result. The actual tree assembly
  is delegated to a shared _TreeAssembler (also used by JsonStreamParser), so
  this type is only the adapter from the batch token callback — reading the
  parser's last_string/last_number side-channel — to assembler calls.
  """
  embed _assembler: _TreeAssembler

  new ref create() =>
    _assembler = _TreeAssembler

  fun ref apply(parser: JsonTokenParser, token: JsonToken) =>
    match \exhaustive\ token
    | JsonTokenObjectStart => _assembler.begin_object()
    | JsonTokenArrayStart => _assembler.begin_array()
    | JsonTokenKey => _assembler.key(parser.last_string)
    | JsonTokenString => _assembler.value(parser.last_string)
    | JsonTokenNumber =>
      match \exhaustive\ parser.last_number
      | let n: I64 => _assembler.value(n)
      | let n: F64 => _assembler.value(n)
      end
    | JsonTokenTrue => _assembler.value(true)
    | JsonTokenFalse => _assembler.value(false)
    | JsonTokenNull => _assembler.value(None)
    | JsonTokenObjectEnd => _assembler.end_object()
    | JsonTokenArrayEnd => _assembler.end_array()
    end

  fun result(): (JsonValue | _NoResult) =>
    _assembler.result()
