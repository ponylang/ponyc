use "pony_check"
use "pony_test"

// ---------------------------------------------------------------------------
// Property-based tests
// ---------------------------------------------------------------------------

class \nodoc\ iso _PropertyBasicAuthFormat
  is Property1[(String val, String val)]
  """
  BasicAuth always returns ("authorization", "Basic <encoded>") where the
  encoded part is non-empty.
  """
  fun name(): String => "basic_auth/format"

  fun gen(): Generator[(String val, String val)] =>
    Generators.zip2[String val, String val](
      Generators.ascii_printable(1, 20),
      Generators.ascii_printable(1, 20))

  fun ref property(
    arg1: (String val, String val),
    ph: PropertyHelper)
  =>
    (let username, let password) = arg1
    (let hdr_name, let hdr_value) = BasicAuth(username, password)
    ph.assert_eq[String val]("authorization", hdr_name)
    ph.assert_true(
      hdr_value.at("Basic "),
      "value should start with 'Basic '")
    ph.assert_true(
      hdr_value.size() > "Basic ".size(),
      "encoded credentials should be non-empty")

class \nodoc\ iso _PropertyBearerAuthFormat
  is Property1[String val]
  """
  BearerAuth always returns ("authorization", "Bearer <token>").
  """
  fun name(): String => "bearer_auth/format"

  fun gen(): Generator[String val] =>
    Generators.ascii_printable(1, 50)

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    (let hdr_name, let hdr_value) = BearerAuth(arg1)
    ph.assert_eq[String val]("authorization", hdr_name)
    ph.assert_eq[String val]("Bearer " + arg1, hdr_value)

// ---------------------------------------------------------------------------
// Example-based tests
// ---------------------------------------------------------------------------
class \nodoc\ iso _TestBasicAuthKnownGood is UnitTest
  """Known Basic auth encoding (RFC 7617 example)."""
  fun name(): String => "basic_auth/known_good"

  fun apply(h: TestHelper) =>
    // "Aladdin:open sesame" -> base64 "QWxhZGRpbjpvcGVuIHNlc2FtZQ=="
    (let hdr_name, let hdr_value) = BasicAuth("Aladdin", "open sesame")
    h.assert_eq[String val]("authorization", hdr_name)
    h.assert_eq[String val](
      "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==", hdr_value)

class \nodoc\ iso _TestBearerAuthKnownGood is UnitTest
  """Known Bearer auth header."""
  fun name(): String => "bearer_auth/known_good"

  fun apply(h: TestHelper) =>
    (let hdr_name, let hdr_value) = BearerAuth("my-token-123")
    h.assert_eq[String val]("authorization", hdr_name)
    h.assert_eq[String val]("Bearer my-token-123", hdr_value)
