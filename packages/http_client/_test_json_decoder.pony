use json = "json"
use "pony_check"
use "pony_test"

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

class \nodoc\ val _Todo
  let id: I64
  let title: String

  new val create(id': I64, title': String) =>
    id = id'
    title = title'

primitive \nodoc\ _TodoDecoder is JSONDecoder[_Todo]
  fun apply(value: json.JSONValue): (_Todo | JSONDecodeError) =>
    let nav = json.JSONNav(value)
    try
      _Todo(nav("id").as_i64()?, nav("title").as_string()?)
    else
      JSONDecodeError(
        "expected object with integer 'id' and string 'title'")
    end

primitive \nodoc\ _AlwaysSucceedDecoder is JSONDecoder[String]
  fun apply(value: json.JSONValue): (String | JSONDecodeError) =>
    "success"

primitive \nodoc\ _AlwaysFailDecoder is JSONDecoder[String]
  fun apply(value: json.JSONValue): (String | JSONDecodeError) =>
    JSONDecodeError("always fails")

// ---------------------------------------------------------------------------
// Generators
// ---------------------------------------------------------------------------
primitive \nodoc\ _InvalidJSONGen
  fun apply(): Generator[String] =>
    Generators.ascii_printable(
      where from = 0, to = 50
    ).map[String]({(s) => "{{INVALID " + s})

primitive \nodoc\ _ValidJSONGen
  fun apply(): Generator[String] =>
    Generators.one_of[String](
      [ "null"
        "true"
        "false"
        "42"
        "-1"
        "3.14"
        "\"hello\""
        "[]"
        "[1,2,3]"
        "{}"
        "{\"a\":1}"
        "{\"a\":\"b\",\"c\":true}"
      ])

// ---------------------------------------------------------------------------
// Property-based tests
// ---------------------------------------------------------------------------
class \nodoc\ iso _PropertyDecodeJSONParseErrorPropagation
  is Property1[String val]
  """
  Invalid JSON always yields JSONParseError, never success or decode error.
  """
  fun name(): String => "json_decoder/property/parse_error_propagation"

  fun gen(): Generator[String val] => _InvalidJSONGen()

  fun property(sample: String val, h: PropertyHelper) =>
    let response = _MakeJSONResponse(sample)
    match \exhaustive\ DecodeJSON[String](response, _AlwaysSucceedDecoder)
    | let s: String =>
      h.fail("expected JSONParseError, got success: " + s)
    | let err: json.JSONParseError => None
    | let err: JSONDecodeError =>
      h.fail("expected JSONParseError, got JSONDecodeError: " + err.string())
    end

class \nodoc\ iso _PropertyDecodeJSONDecodeErrorPropagation
  is Property1[String val]
  """Valid JSON with always-fail decoder yields JSONDecodeError."""
  fun name(): String => "json_decoder/property/decode_error_propagation"

  fun gen(): Generator[String val] => _ValidJSONGen()

  fun property(sample: String val, h: PropertyHelper) =>
    let response = _MakeJSONResponse(sample)
    match \exhaustive\ DecodeJSON[String](response, _AlwaysFailDecoder)
    | let s: String =>
      h.fail("expected JSONDecodeError, got success: " + s)
    | let err: json.JSONParseError =>
      h.fail("expected JSONDecodeError, got JSONParseError: " + err.string())
    | let err: JSONDecodeError => None
    end

class \nodoc\ iso _PropertyDecodeJSONIdentityDecoder is Property1[String val]
  """Valid JSON with always-succeed decoder yields success."""
  fun name(): String => "json_decoder/property/identity_decoder"

  fun gen(): Generator[String val] => _ValidJSONGen()

  fun property(sample: String val, h: PropertyHelper) =>
    let response = _MakeJSONResponse(sample)
    match \exhaustive\ DecodeJSON[String](response, _AlwaysSucceedDecoder)
    | let s: String => None
    | let err: json.JSONParseError =>
      h.fail("expected success, got JSONParseError: " + err.string())
    | let err: JSONDecodeError =>
      h.fail("expected success, got JSONDecodeError: " + err.string())
    end

// ---------------------------------------------------------------------------
// Example-based tests
// ---------------------------------------------------------------------------
class \nodoc\ iso _TestJSONDecoderSuccessfulDecode is UnitTest
  """Valid JSON matching the decoder schema decodes to the expected type."""
  fun name(): String => "json_decoder/successful_decode"

  fun apply(h: TestHelper) =>
    let value = json.JSONObject
      .update("id", I64(1))
      .update("title", "test")
    match \exhaustive\ _TodoDecoder(value)
    | let todo: _Todo =>
      h.assert_eq[I64](1, todo.id)
      h.assert_eq[String val]("test", todo.title)
    | let err: JSONDecodeError =>
      h.fail("expected success: " + err.string())
    end

class \nodoc\ iso _TestJSONDecoderMissingField is UnitTest
  """JSON missing a required field yields JSONDecodeError."""
  fun name(): String => "json_decoder/missing_field"

  fun apply(h: TestHelper) =>
    let value = json.JSONObject
      .update("id", I64(1))
    match \exhaustive\ _TodoDecoder(value)
    | let todo: _Todo =>
      h.fail("expected JSONDecodeError for missing 'title'")
    | let err: JSONDecodeError =>
      h.assert_true(err.message.size() > 0, "error should have a message")
    end

class \nodoc\ iso _TestJSONDecoderWrongType is UnitTest
  """JSON with a field of the wrong type yields JSONDecodeError."""
  fun name(): String => "json_decoder/wrong_type"

  fun apply(h: TestHelper) =>
    let value = json.JSONObject
      .update("id", "not_a_number")
      .update("title", "test")
    match \exhaustive\ _TodoDecoder(value)
    | let todo: _Todo =>
      h.fail("expected JSONDecodeError for wrong type on 'id'")
    | let err: JSONDecodeError =>
      h.assert_true(err.message.size() > 0, "error should have a message")
    end

class \nodoc\ iso _TestDecodeJSONValidJSON is UnitTest
  """Full pipeline: valid JSON response decodes to typed domain object."""
  fun name(): String => "decode_json/valid_json"

  fun apply(h: TestHelper) =>
    let response = _MakeJSONResponse("{\"id\": 42, \"title\": \"hello\"}")
    match \exhaustive\ DecodeJSON[_Todo](response, _TodoDecoder)
    | let todo: _Todo =>
      h.assert_eq[I64](42, todo.id)
      h.assert_eq[String val]("hello", todo.title)
    | let err: json.JSONParseError =>
      h.fail("expected success, got JSONParseError: " + err.string())
    | let err: JSONDecodeError =>
      h.fail("expected success, got JSONDecodeError: " + err.string())
    end

class \nodoc\ iso _TestDecodeJSONInvalidJSON is UnitTest
  """Non-JSON body yields JSONParseError through DecodeJSON."""
  fun name(): String => "decode_json/invalid_json"

  fun apply(h: TestHelper) =>
    let response = _MakeJSONResponse("not json {{")
    match \exhaustive\ DecodeJSON[_Todo](response, _TodoDecoder)
    | let todo: _Todo =>
      h.fail("expected JSONParseError")
    | let err: json.JSONParseError =>
      h.assert_true(err.message.size() > 0, "error should have a message")
    | let err: JSONDecodeError =>
      h.fail("expected JSONParseError, got JSONDecodeError: " + err.string())
    end

class \nodoc\ iso _TestDecodeJSONWrongStructure is UnitTest
  """
  Valid JSON with wrong structure yields JSONDecodeError through DecodeJSON.
  """
  fun name(): String => "decode_json/wrong_structure"

  fun apply(h: TestHelper) =>
    let response = _MakeJSONResponse("[1, 2, 3]")
    match \exhaustive\ DecodeJSON[_Todo](response, _TodoDecoder)
    | let todo: _Todo =>
      h.fail("expected JSONDecodeError")
    | let err: json.JSONParseError =>
      h.fail("expected JSONDecodeError, got JSONParseError: " + err.string())
    | let err: JSONDecodeError =>
      h.assert_true(err.message.size() > 0, "error should have a message")
    end

class \nodoc\ iso _TestJSONDecodeErrorString is UnitTest
  """`JSONDecodeError.string()` returns the error message."""
  fun name(): String => "json_decode_error/string"

  fun apply(h: TestHelper) =>
    let err = JSONDecodeError("msg")
    h.assert_eq[String val]("msg", err.string())
