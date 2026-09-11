use "pony_check"
use "pony_test"

// ---------------------------------------------------------------------------
// Property-based tests
// ---------------------------------------------------------------------------

class \nodoc\ iso _PropertySerializerContainsMethod
  is Property1[String val]
  """Method string appears in the serialized request line."""
  fun name(): String => "serializer/contains_method"

  fun gen(): Generator[String val] =>
    Generators.one_of[String val](
      [ "GET"; "POST"; "PUT"; "DELETE"; "HEAD"
        "OPTIONS"; "PATCH"; "TRACE"])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    let method =
      match Methods.parse(arg1)
      | let m: Method => m
      else
        ph.fail("should parse: " + arg1)
        return
      end
    let request = HTTPRequest(method, "/test")
    let serialized: String val =
      String.from_iso_array(_RequestSerializer(request, "example.com", "80"))
    // Request line starts with METHOD
    ph.assert_true(
      serialized.contains(arg1 + " /test HTTP/1.1\r\n"),
      "request line should contain method: " + arg1)

class \nodoc\ iso _PropertySerializerContainsPath
  is Property1[String val]
  """Path appears in the serialized request line."""
  fun name(): String => "serializer/contains_path"

  fun gen(): Generator[String val] =>
    Generators.map2[String val, String val, String val](
      Generators.unit[String val]("/"),
      Generators.ascii_letters(0, 20),
      {(slash, rest) => slash + rest })

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    let request = HTTPRequest(GET, arg1)
    let serialized: String val =
      String.from_iso_array(_RequestSerializer(request, "example.com", "80"))
    ph.assert_true(
      serialized.contains("GET " + arg1 + " HTTP/1.1"),
      "request line should contain path: " + arg1)

class \nodoc\ iso _PropertySerializerAutoHost
  is Property1[String val]
  """Host header is auto-set from host parameter."""
  fun name(): String => "serializer/auto_host"

  fun gen(): Generator[String val] =>
    Generators.one_of[String val](
      ["example.com"; "api.test.org"; "localhost"])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    let request = HTTPRequest(GET, "/")
    let serialized: String val =
      String.from_iso_array(_RequestSerializer(request, arg1, "80"))
    ph.assert_true(
      serialized.contains("Host: " + arg1 + "\r\n"),
      "should contain Host: " + arg1)

class \nodoc\ iso _PropertySerializerAutoContentLength
  is Property1[USize]
  """Content-Length is auto-set from body size."""
  fun name(): String => "serializer/auto_content_length"

  fun gen(): Generator[USize] =>
    Generators.usize(1, 100)

  fun ref property(arg1: USize, ph: PropertyHelper) =>
    let body =
      recover val
        let b = Array[U8](arg1)
        var i: USize = 0
        while i < arg1 do
          b.push('X')
          i = i + 1
        end
        b
      end
    let request = HTTPRequest(POST, "/data" where body' = body)
    let serialized: String val =
      String.from_iso_array(
        _RequestSerializer(request, "example.com", "80"))
    ph.assert_true(
      serialized.contains(
        "Content-Length: " + arg1.string() + "\r\n"),
      "should contain Content-Length: " + arg1.string())

// ---------------------------------------------------------------------------
// Example-based tests
// ---------------------------------------------------------------------------
class \nodoc\ iso _TestSerializerKnownGood is UnitTest
  """Known request -> known wire bytes."""
  fun name(): String => "serializer/known_good"

  fun apply(h: TestHelper) =>
    let request = HTTPRequest(GET, "/index.html")
    let serialized: String val =
      String.from_iso_array(
        _RequestSerializer(request, "www.example.com", "80"))

    let expected: String val =
      "GET /index.html HTTP/1.1\r\n" +
      "Host: www.example.com\r\n" +
      "\r\n"
    h.assert_eq[String val](expected, serialized)

class \nodoc\ iso _TestSerializerHostWithPort is UnitTest
  """Non-standard port included in Host header."""
  fun name(): String => "serializer/host_with_port"

  fun apply(h: TestHelper) =>
    let request = HTTPRequest(GET, "/")
    let serialized: String val =
      String.from_iso_array(
        _RequestSerializer(request, "example.com", "8080"))
    h.assert_true(
      serialized.contains("Host: example.com:8080\r\n"),
      "Host should include port 8080")

class \nodoc\ iso _TestSerializerHostDefaultPort is UnitTest
  """Port 80 and 443 omitted from Host header."""
  fun name(): String => "serializer/host_default_port"

  fun apply(h: TestHelper) =>
    // Port 80
    let s80: String val =
      String.from_iso_array(
        _RequestSerializer(HTTPRequest(GET, "/"), "example.com", "80"))
    h.assert_true(
      s80.contains("Host: example.com\r\n"),
      "port 80 should be omitted")
    h.assert_false(
      s80.contains("Host: example.com:80\r\n"),
      "port 80 should not appear")

    // Port 443
    let s443: String val =
      String.from_iso_array(
        _RequestSerializer(HTTPRequest(GET, "/"), "example.com", "443"))
    h.assert_true(
      s443.contains("Host: example.com\r\n"),
      "port 443 should be omitted")

class \nodoc\ iso _TestSerializerUserHostTakesPrecedence is UnitTest
  """Explicit Host header is not overwritten."""
  fun name(): String => "serializer/user_host_precedence"

  fun apply(h: TestHelper) =>
    let hdrs =
      recover val
        Headers
          .> set("Host", "custom.host.com")
      end
    let request = HTTPRequest(GET, "/" where headers' = hdrs)
    let serialized: String val =
      String.from_iso_array(
        _RequestSerializer(request, "example.com", "80"))
    h.assert_true(
      serialized.contains("host: custom.host.com\r\n"),
      "should use user-provided Host")
    // Should NOT also have auto-generated Host
    h.assert_false(
      serialized.contains("Host: example.com"),
      "should not auto-generate Host when user provided one")

class \nodoc\ iso _TestSerializerUserContentLengthTakesPrecedence
  is UnitTest
  """Explicit Content-Length is not overwritten."""
  fun name(): String => "serializer/user_content_length_precedence"

  fun apply(h: TestHelper) =>
    let body: Array[U8] val = [as U8: 'H'; 'i']
    let hdrs =
      recover val
        Headers
          .> set("Content-Length", "999")
      end
    let request =
      HTTPRequest(
        POST, "/data"
        where headers' = hdrs, body' = body)
    let serialized: String val =
      String.from_iso_array(
        _RequestSerializer(request, "example.com", "80"))
    h.assert_true(
      serialized.contains("content-length: 999\r\n"),
      "should use user-provided Content-Length")

class \nodoc\ iso _TestSerializerNoBody is UnitTest
  """No body -> no Content-Length."""
  fun name(): String => "serializer/no_body"

  fun apply(h: TestHelper) =>
    let request = HTTPRequest(GET, "/")
    let serialized: String val =
      String.from_iso_array(
        _RequestSerializer(request, "example.com", "80"))
    h.assert_false(
      serialized.contains("Content-Length"),
      "GET with no body should not have Content-Length")
