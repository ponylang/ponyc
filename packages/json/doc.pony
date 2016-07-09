use "collections"

class JsonDoc
  """
  Top level JSON type containing an entire document.
  A JSON document consists of exactly 1 value.
  """
  var data: JsonType

  // Internal state for parsing
  var _source: String = ""
  var _index: USize = 0
  var _line: USize = 1
  var _err_line: USize = 0    // Error information from last parse
  var _err_msg: String = "" // ..
  var _last_index: USize = 0  // Last source index we peeked or got, for errors

  new iso create() =>
    """
    Default constructor building a document containing a single null.
    """
    data = None

  fun string(pretty_print: Bool = false): String =>
    """
    Generate string representation of this document.
    """
    _JsonPrint._string(data, "", pretty_print)

  fun ref parse(source: String) ? =>
    """
    Parse the given string as a JSON file, building a document.
    Raise error on invalid JSON in given source.
    """
    _source = source
    _index = 0
    _line = 1
    _err_line = 0
    _err_msg = ""
    _last_index = 0

    data = _parse_value("top level value")

    // Make sure there's no trailing text
    _dump_whitespace()
    if _index < _source.size() then
      _peek_char()  // Setup _last_index
      _error("Unexpected text found after top level value: " + _last_char())
      error
    end

  fun parse_report(): (USize /* line */, String /*message */) =>
    """
    Give details of the error that occured last time we attempted to parse.
    If parse was successful returns (0, "").
    """
    (_err_line, _err_msg)

  fun ref _parse_value(context: String): JsonType ? =>
    """
    Parse a single JSON value of any type, which MUST be present.
    Raise error on invalid or missing value.
    """
    _dump_whitespace()
    match _peek_char(context)
    | let c: U8 if (c >= 'a') and (c <= 'z') => _parse_keyword()
    | let c: U8 if (c >= '0') and (c <= '9') => _parse_number()
    | '-' => _parse_number()
    | '{' => _parse_object()
    | '[' => _parse_array()
    | '"' => _parse_string("string value")
    else
      _error("Unexpected character '" + _last_char() + "'")
      error
    end

  fun ref _parse_keyword(): (Bool | None) ? =>
    """
    Parse a keyword, the first letter of which has already been peeked.
    """
    var word: String ref = String

    // Find the contiguous block of lower case letters
    while let c = _peek_char(); (c >= 'a') and (c <= 'z') do
      word.push(c)
      _get_char() // Consume peeked char
    end

    match word
    | "true" =>  true
    | "false" => false
    | "null" =>  None
    else
      _error("Unrecognised keyword \"" + word + "\"")
      error
    end

  fun ref _parse_number(): (F64 | I64) ? =>
    """
    Parse a number, the leading character of which has already been peeked.
    """
    var minus = false

    if _peek_char("number") == '-' then
      minus = true
      _get_char() // Consume -
    end

    var frac: I64 = 0
    var frac_digits: U8 = 0
    var exp: I64 = 0
    var exp_digits: U8 = 0

    // Start with integer part
    (let int, _) = _parse_decimal()

    if _peek_char() == '.' then
      // We have a . so expect a fractional part
      _get_char()  // Consume .
      (frac, frac_digits) = _parse_decimal()
    end

    if (_peek_char() or 0x20) == 'e' then
      // We have an e so expect an exponent
      _get_char()  // Consume e
      var neg_exp = false

      match _peek_char("number exponent")
      | '-' => _get_char(); neg_exp = true
      | '+' => _get_char()
      end

      (exp, exp_digits) = _parse_decimal()

      if neg_exp then
        exp = -exp
      end
    end

    if (frac_digits == 0) and (exp_digits == 0) then
      // Just an integer
      return if minus then -int else int end
    end

    // We have fractional part and/or exponent, make a float
    var f = (int.f64() + (frac.f64() / F64(10).pow(frac_digits.f64()))) *
      (F64(10).pow(exp.f64()))

    if minus then -f else f end

  fun ref _parse_decimal(): (I64 /* value */, U8 /* digit count */) ? =>
    """
    Parse a decimal integer which must appear immediately in the source.
    """
    var value: I64 = 0
    var digit_count: U8 = 0
    var c = _peek_char("number")

    while (c >= '0') and (c <= '9') do
      _get_char() // Consume peeked digit
      value = (value * 10) + (c - '0').i64()
      digit_count = digit_count + 1
      c = _peek_char()
    end

    if digit_count == 0 then
      _error("Expected number got '" + _last_char() + "'")
      error
    end

    (value, digit_count)

  fun ref _parse_object(): JsonObject ? =>
    """
    Parse a JSON object, the leading { of which has already been peeked.
    """
    _get_char() // Consume {
    _dump_whitespace()

    if _peek_char("object") == '}' then
      // Empty object
      _get_char() // Consume }
      return JsonObject
    end

    let map = Map[String, JsonType]

    // Find elements in object
    while true do
      // Each element of of the form:
      //   "key": value
      let key = _parse_string("object key")
      _dump_whitespace()

      if _get_char("object element value") != ':' then
        _error("Expected ':' after object key, got '" + _last_char() + "'")
        error
      end

      map(key) = _parse_value("object")
      _dump_whitespace()

      // Must now have another element, separated by a comma, or the end of the
      // object
      match _get_char("object")
      | '}' => break // End of object
      | ',' => None  // Next element
      else
        _error("Expected ',' after object element, got '" + _last_char() + "'")
        error
      end
    end

    JsonObject.from_map(map)

  fun ref _parse_array(): JsonArray ? =>
    """
    Parse an array, the leading [ of which has already been peeked.
    """
    _get_char() // Consume [
    _dump_whitespace()

    if _peek_char("array") == ']' then
      // Empty array
      _get_char() // Consume ]
      return JsonArray
    end

    let array = Array[JsonType]

    // Find elements in array
    while true do
      array.push(_parse_value("array"))
      _dump_whitespace()

      // Must now have another element, separated by a comma, or the end of the
      // array
      match _get_char("array")
      | ']' => break // End of array
      | ',' => None // Next element
      else
        _error("Expected ',' after array element, got '" + _last_char() + "'")
        error
      end
    end

    JsonArray.from_array(array)

  fun ref _parse_string(context: String): String ? =>
    """
    Parse a string, which must be the next thing found, other than whitesapce.
    """
    _dump_whitespace()

    if _get_char(context) != '"' then
      _error("Expected " + context + ", got '" + _last_char() + "'")
      error
    end

    var text = recover iso String end

    // Process characters one at a time until we hit the end "
    while let c = _get_char(context); c != '"' do
      if c == '\\' then
        text.append(_parse_escape())
      else
        text.push(c)
      end
    end

    text

  fun ref _parse_escape(): String ? =>
    """
    Process a string escape sequence, the leading \ of which has already been
    consumed.
    """
    match _get_char("escape sequence")
    | '"' => "\""
    | '\\' => "\\"
    | '/' => "/"
    | 'b' => "\b"
    | 'f' => "\f"
    | 'n' => "\n"
    | 'r' => "\r"
    | 't' => "\t"
    | 'u' => _parse_unicode_escape()
    else
      _error("Unrecognised escape sequence \\" + _last_char())
      error
    end

  fun ref _parse_unicode_escape(): String ? =>
    """
    Process a Unicode escape sequence, the leading \u of which has already been
    consumed.
    """
    let value = _parse_unicode_digits()

    if (value < 0xD800) or (value >= 0xE000) then
      // Just a simple UTF-16 character
      return recover val String.from_utf32(value) end
    end

    // Value is one half of a UTF-16 surrogate pair, get the other half
    if (_get_char("Unicode escape sequence") != '\\') or
      (_get_char("Unicode escape sequence") != 'u') then
      _error("Expected UTF-16 trailing surrogate, got '" + _last_char() + "'")
      error
    end

    let trailing = _parse_unicode_digits()
    let fmt = FormatSettingsInt.set_format(FormatHexBare).set_width(4)

    if (value >= 0xDC00) or (trailing < 0xDC00) or (trailing >= 0xE000) then
      _error("Expected UTF-16 surrogate pair, got \\u" + value.string(fmt) +
        " \\u" + trailing.string(fmt))
      error
    end

    // Have both surrogates, combine them
    let c = 0x10000 + ((value and 0x3FF) << 10) + (trailing and 0x3FF)
    recover val String.from_utf32(c) end

  fun ref _parse_unicode_digits(): U32 ? =>
    """
    Parse the hex digits of a Unicode escape sequence, the leading \u of which
    has already been consumed, and return the encoded character value.
    """
    var value: U32 = 0
    var i: U8 = 0

    while i < 4 do
      let d =
        match _get_char("Unicode escape sequence")
        | let c: U8 if (c >= '0') and (c <= '9') => c - '0'
        | let c: U8 if (c >= 'a') and (c <= 'f') => (c - 'a') + 10
        | let c: U8 if (c >= 'A') and (c <= 'F') => (c - 'A') + 10
        else
          _error("Invalid character '" + _last_char() +
            "' in Unicode escape sequence")
          error
        end

      value = (value * 16) + d.u32()
      i = i + 1
    end

    value

  fun ref _dump_whitespace() =>
    """
    Dump all whitespace at the current read location in source, if any.
    """
    try
      while true do
        match _source(_index)
        | ' '
        | '\r'
        | '\t' => None
        | '\n' => _line = _line + 1
        else
          // Non whitespace found
          return
        end

        _index = _index + 1
      end
    end

  fun ref _peek_char(eof_context: (String | None) = None): U8 ? =>
    """
    Peek the next char in the source, without consuming it.
    If an eof_context is given then an error is thrown on eof, setting a
    suitable message.
    If eof_context is None then 0 is returned on EOF. It up to the caller to
    handle this appropriately.
    """
    try
      _last_index = _index
      _source(_index)
    else
      // EOF found, is that OK?
      _last_index = -1

      match eof_context
      | None => return 0  // EOF is allowed
      | let context: String =>
        // EOF not allowed
        _error("Unexpected EOF in " + context)
      end

      // This error really should be inside the match above, but that gives us
      // a bad return type until exhaustive matches are implemented
      error
    end

  fun ref _get_char(eof_context: (String | None) = None): U8 ? =>
    """
    Get and consume the next char in the source.
    If an eof_context is given then an error is thrown on eof, setting a
    suitable message.
    If eof_context is None then 0 is returned on EOF. It up to the caller to
    handle this appropriately.
    """
    let c = _peek_char(eof_context)

    if c == '\n' then
      _line = _line + 1
    end

    _index = _index + 1
    c

  fun ref _last_char(): String =>
    """
    Get the last character peeked or got from the source as a String.
    For use generating error messages.
    """
    if _last_index == -1 then
      "EOF"
    else
      _source.substring(_last_index.isize(), _last_index.isize() + 1)
    end

  fun ref _error(msg: String) =>
    """
    Record an error with the given message.
    """
    _err_line = _line
    _err_msg = msg
