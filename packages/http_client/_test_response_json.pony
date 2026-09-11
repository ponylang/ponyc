use json = "json"
use "pony_test"

primitive \nodoc\ _MakeJSONResponse
  fun apply(body_str: String): HTTPResponse val =>
    let body: Array[U8] val = body_str.array()
    HTTPResponse(HTTP11, 200, "OK", recover val Headers end, body)

// ---------------------------------------------------------------------------
// Example-based tests
// ---------------------------------------------------------------------------
class \nodoc\ iso _TestResponseJSONValidObject is UnitTest
  """Valid JSON object body parses to expected structure."""
  fun name(): String => "response_json/valid_object"

  fun apply(h: TestHelper) =>
    let response = _MakeJSONResponse("{\"name\": \"Alice\", \"age\": 30}")
    match \exhaustive\ ResponseJSON(response)
    | let value: json.JSONValue =>
      match value
      | let obj: json.JSONObject =>
        try
          match obj("name")?
          | let s: String => h.assert_eq[String val]("Alice", s)
          else h.fail("name should be a string")
          end
          match obj("age")?
          | let n: I64 => h.assert_eq[I64](30, n)
          else h.fail("age should be an integer")
          end
        else
          h.fail("key access should not error")
        end
      else
        h.fail("should parse as JSONObject")
      end
    | let err: json.JSONParseError =>
      h.fail("should not fail: " + err.string())
    end

class \nodoc\ iso _TestResponseJSONValidArray is UnitTest
  """Valid JSON array body parses correctly."""
  fun name(): String => "response_json/valid_array"

  fun apply(h: TestHelper) =>
    let response = _MakeJSONResponse("[1, 2, 3]")
    match \exhaustive\ ResponseJSON(response)
    | let value: json.JSONValue =>
      match value
      | let arr: json.JSONArray =>
        h.assert_eq[USize](3, arr.size())
      else
        h.fail("should parse as JSONArray")
      end
    | let err: json.JSONParseError =>
      h.fail("should not fail: " + err.string())
    end

class \nodoc\ iso _TestResponseJSONInvalid is UnitTest
  """Invalid JSON body returns JSONParseError."""
  fun name(): String => "response_json/invalid"

  fun apply(h: TestHelper) =>
    let response = _MakeJSONResponse("not json {{")
    match \exhaustive\ ResponseJSON(response)
    | let value: json.JSONValue =>
      h.fail("should fail on invalid JSON")
    | let err: json.JSONParseError =>
      h.assert_true(err.message.size() > 0, "error should have a message")
    end

class \nodoc\ iso _TestResponseJSONEmptyBody is UnitTest
  """Empty body returns JSONParseError."""
  fun name(): String => "response_json/empty_body"

  fun apply(h: TestHelper) =>
    let response = _MakeJSONResponse("")
    match \exhaustive\ ResponseJSON(response)
    | let value: json.JSONValue =>
      h.fail("empty body should fail to parse")
    | let err: json.JSONParseError =>
      h.assert_true(err.message.size() > 0, "error should have a message")
    end
