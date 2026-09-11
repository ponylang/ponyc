use "pony_check"
use "pony_test"

// ---------------------------------------------------------------------------
// Property-based tests
// ---------------------------------------------------------------------------

class \nodoc\ iso _PropertyQueryUnreservedPassthrough
  is Property1[String val]
  """
  RFC 3986 unreserved characters (A-Z a-z 0-9 - . _ ~) pass through
  query encoding unchanged.
  """
  fun name(): String => "percent_encoder/query_unreserved_passthrough"

  fun gen(): Generator[String val] =>
    let unreserved =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~"
    Generators.ascii(1, 50 where range = ASCIIAll)
      .filter({(s) =>
        var ok = true
        for byte in s.values() do
          var found = false
          for u in unreserved.values() do
            if byte == u then found = true; break end
          end
          if not found then ok = false; break end
        end
        (s, ok)
      })

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    let encoded: String val = _PercentEncoder.query(arg1)
    ph.assert_eq[String val](arg1, encoded)

class \nodoc\ iso _PropertyQueryReservedEncoded
  is Property1[U8]
  """
  Reserved/non-unreserved bytes are encoded as %XX in query encoding.
  """
  fun name(): String => "percent_encoder/query_reserved_encoded"

  fun gen(): Generator[U8] =>
    Generators.u8().filter({(byte) =>
      let is_unreserved =
        ((byte >= 'A') and (byte <= 'Z')) or
          ((byte >= 'a') and (byte <= 'z')) or
          ((byte >= '0') and (byte <= '9')) or
          (byte == '-') or (byte == '.') or (byte == '_') or (byte == '~')
      (byte, not is_unreserved)
    })

  fun ref property(arg1: U8, ph: PropertyHelper) =>
    let input = recover val String .> push(arg1) end
    let encoded: String val = _PercentEncoder.query(input)
    ph.assert_eq[USize](
      3,
      encoded.size(),
      "reserved byte should encode to 3-char %XX")
    try
      ph.assert_eq[U8](
        '%',
        encoded(0)?,
        "first char should be %")
    else
      ph.fail("could not read encoded output")
    end

class \nodoc\ iso _PropertyFormSpacesToPlus
  is Property1[USize]
  """
  Spaces in form encoding become '+'.
  """
  fun name(): String => "percent_encoder/form_spaces_to_plus"

  fun gen(): Generator[USize] =>
    Generators.usize(1, 20)

  fun ref property(arg1: USize, ph: PropertyHelper) =>
    var input = recover iso String(arg1) end
    var i: USize = 0
    while i < arg1 do
      input.push(' ')
      i = i + 1
    end
    let encoded: String val = _PercentEncoder.form(consume input)
    ph.assert_eq[USize](arg1, encoded.size())
    for byte in encoded.values() do
      ph.assert_eq[U8]('+', byte, "spaces should become +")
    end

class \nodoc\ iso _PropertyQueryParamsRoundtrip
  is Property1[(String val, String val)]
  """
  QueryParams output contains the encoded key and value separated by =.
  """
  fun name(): String => "query_params/contains_encoded_pair"

  fun gen(): Generator[(String val, String val)] =>
    Generators.zip2[String val, String val](
      Generators.ascii_letters(1, 20),
      Generators.ascii_letters(1, 20))

  fun ref property(
    arg1: (String val, String val),
    ph: PropertyHelper)
  =>
    (let key, let value) = arg1
    let params = recover val [(key, value)] end
    let result = QueryParams(params)
    // ASCII letters are unreserved, so they pass through unchanged
    ph.assert_eq[String val](key + "=" + value, result)

// ---------------------------------------------------------------------------
// Example-based tests
// ---------------------------------------------------------------------------
class \nodoc\ iso _TestQueryParamsKnownGood is UnitTest
  """Known query string encoding."""
  fun name(): String => "query_params/known_good"

  fun apply(h: TestHelper) =>
    let params =
      recover val
        [("q", "hello world"); ("page", "1")]
      end
    let result = QueryParams(params)
    h.assert_eq[String val]("q=hello%20world&page=1", result)

class \nodoc\ iso _TestQueryParamsEmpty is UnitTest
  """Empty params returns empty string."""
  fun name(): String => "query_params/empty"

  fun apply(h: TestHelper) =>
    let params = recover val Array[(String, String)] end
    h.assert_eq[String val]("", QueryParams(params))

class \nodoc\ iso _TestQueryParamsSpecialChars is UnitTest
  """Special characters are percent-encoded."""
  fun name(): String => "query_params/special_chars"

  fun apply(h: TestHelper) =>
    let params = recover val [("key", "a&b=c")] end
    let result = QueryParams(params)
    h.assert_eq[String val]("key=a%26b%3Dc", result)

class \nodoc\ iso _TestFormEncoderKnownGood is UnitTest
  """Known form encoding output."""
  fun name(): String => "form_encoder/known_good"

  fun apply(h: TestHelper) =>
    let params =
      recover val
        [("name", "John Doe"); ("age", "30")]
      end
    let result = FormEncoder(params)
    let expected: String val = "name=John+Doe&age=30"
    h.assert_eq[String val](expected, String.from_array(result))

class \nodoc\ iso _TestFormEncoderEmpty is UnitTest
  """Empty params returns empty array."""
  fun name(): String => "form_encoder/empty"

  fun apply(h: TestHelper) =>
    let params = recover val Array[(String, String)] end
    h.assert_eq[USize](0, FormEncoder(params).size())

class \nodoc\ iso _TestFormEncoderSpecialChars is UnitTest
  """Form encoding uses + for spaces and %XX for other specials."""
  fun name(): String => "form_encoder/special_chars"

  fun apply(h: TestHelper) =>
    let params = recover val [("q", "a b&c")] end
    let result = String.from_array(FormEncoder(params))
    h.assert_eq[String val]("q=a+b%26c", result)
