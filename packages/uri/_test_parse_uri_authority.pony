use "pony_test"
use "pony_check"

class \nodoc\ iso _PropertyAuthorityRoundtrip
  is Property1[_ValidAuthorityInput]
  """
  For generated valid authorities, ParseURIAuthority(auth.string()) produces
  an equal authority.
  """
  fun name(): String => "uri/parse_uri_authority/roundtrip"

  fun gen(): Generator[_ValidAuthorityInput] =>
    _ValidAuthorityInputGenerator()

  fun ref property(arg1: _ValidAuthorityInput, ph: PropertyHelper) =>
    let original = URIAuthority(arg1.userinfo, arg1.host, arg1.port)
    let serialized = original.string()
    match \exhaustive\ ParseURIAuthority(consume serialized)
    | let reparsed: URIAuthority val =>
      ph.assert_true(
        original == reparsed,
        "roundtrip failed for: " + original.string())
    | let err: URIParseError val =>
      ph.fail("roundtrip parse failed for: " + original.string() +
        " error: " + err.string())
    end

class \nodoc\ iso _PropertyInvalidPortRejected is Property1[String val]
  """Invalid ports (non-numeric, > 65535) produce InvalidPort."""
  fun name(): String => "uri/parse_uri_authority/invalid_port"

  fun gen(): Generator[String val] =>
    Generators.frequency[String val](
      [
        as WeightedGenerator[String val]:
        // non-numeric port
        (1, Generators.one_of[String val](
          ["host:abc"; "host:12a"; "host:xy9z"]))
        // port > 65535
        (1, Generators.one_of[String val](
          ["host:65536"; "host:99999"; "host:100000"]))
      ])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    match \exhaustive\ ParseURIAuthority(arg1)
    | let a: URIAuthority val =>
      ph.fail("expected InvalidPort for: " + arg1)
    | let err: URIParseError val =>
      ph.assert_true(
        err is InvalidPort,
        "expected InvalidPort, got: " + err.string() + " for: " + arg1)
    end

class \nodoc\ iso _PropertyInvalidHostRejected is Property1[String val]
  """Malformed IPv6 hosts (unmatched brackets) produce InvalidHost."""
  fun name(): String => "uri/parse_uri_authority/invalid_host"

  fun gen(): Generator[String val] =>
    Generators.one_of[String val](
      [
        // unmatched opening bracket
        "[::1"
        // unmatched with port
        "[::1:8080"
        // empty brackets
        "[]"
        // garbage after closing bracket (not a port)
        "[::1]garbage"
      ])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    match \exhaustive\ ParseURIAuthority(arg1)
    | let a: URIAuthority val =>
      ph.fail("expected InvalidHost for: " + arg1)
    | let err: URIParseError val =>
      ph.assert_true(
        err is InvalidHost,
        "expected InvalidHost, got: " + err.string() + " for: " + arg1)
    end

class \nodoc\ iso _TestParseURIAuthorityKnownGood is UnitTest
  """Known authority parsing cases."""
  fun name(): String => "uri/parse_uri_authority/known_good"

  fun ref apply(h: TestHelper) =>
    // Simple host
    match \exhaustive\ ParseURIAuthority("example.com")
    | let a: URIAuthority val =>
      h.assert_true(a.userinfo is None)
      h.assert_eq[String val]("example.com", a.host)
      h.assert_true(a.port is None)
    | let e: URIParseError val =>
      h.fail("parse failed: " + e.string())
    end

    // Host with port
    match \exhaustive\ ParseURIAuthority("example.com:8080")
    | let a: URIAuthority val =>
      h.assert_true(a.userinfo is None)
      h.assert_eq[String val]("example.com", a.host)
      match a.port
      | let p: U16 => h.assert_eq[U16](8080, p)
      else h.fail("expected port")
      end
    | let e: URIParseError val =>
      h.fail("parse failed: " + e.string())
    end

    // Userinfo
    match \exhaustive\ ParseURIAuthority("user:pass@example.com:443")
    | let a: URIAuthority val =>
      match a.userinfo
      | let u: String => h.assert_eq[String val]("user:pass", u)
      else h.fail("expected userinfo")
      end
      h.assert_eq[String val]("example.com", a.host)
      match a.port
      | let p: U16 => h.assert_eq[U16](443, p)
      else h.fail("expected port")
      end
    | let e: URIParseError val =>
      h.fail("parse failed: " + e.string())
    end

    // IPv6 host
    match \exhaustive\ ParseURIAuthority("[::1]:8080")
    | let a: URIAuthority val =>
      h.assert_true(a.userinfo is None)
      h.assert_eq[String val]("[::1]", a.host)
      match a.port
      | let p: U16 => h.assert_eq[U16](8080, p)
      else h.fail("expected port")
      end
    | let e: URIParseError val =>
      h.fail("parse failed for [::1]:8080: " + e.string())
    end

    // IPv6 without port
    match \exhaustive\ ParseURIAuthority("[2001:db8::7]")
    | let a: URIAuthority val =>
      h.assert_eq[String val]("[2001:db8::7]", a.host)
      h.assert_true(a.port is None)
    | let e: URIParseError val =>
      h.fail("parse failed for [2001:db8::7]: " + e.string())
    end

    // Empty host (used in file:/// URIs)
    match \exhaustive\ ParseURIAuthority("")
    | let a: URIAuthority val =>
      h.assert_eq[String val]("", a.host)
      h.assert_true(a.port is None)
    | let e: URIParseError val =>
      h.fail("parse failed for empty: " + e.string())
    end

    // Port at boundary values
    match \exhaustive\ ParseURIAuthority("host:0")
    | let a: URIAuthority val =>
      match a.port
      | let p: U16 => h.assert_eq[U16](0, p)
      else h.fail("expected port 0")
      end
    | let e: URIParseError val =>
      h.fail("parse failed for port 0: " + e.string())
    end

    match \exhaustive\ ParseURIAuthority("host:65535")
    | let a: URIAuthority val =>
      match a.port
      | let p: U16 => h.assert_eq[U16](65535, p)
      else h.fail("expected port 65535")
      end
    | let e: URIParseError val =>
      h.fail("parse failed for port 65535: " + e.string())
    end

    // Empty port (RFC 3986 allows port = *DIGIT, so empty is valid)
    match \exhaustive\ ParseURIAuthority("host:")
    | let a: URIAuthority val =>
      h.assert_eq[String val]("host", a.host)
      h.assert_true(
        a.port is None, "empty port should parse as None")
    | let e: URIParseError val =>
      h.fail("parse failed for empty port: " + e.string())
    end

// -- Generators --
class \nodoc\ val _ValidAuthorityInput
  let userinfo: (String | None)
  let host: String
  let port: (U16 | None)

  new val create(
    userinfo': (String | None),
    host': String,
    port': (U16 | None))
  =>
    userinfo = userinfo'
    host = host'
    port = port'

primitive \nodoc\ _ValidAuthorityInputGenerator
  fun apply(): Generator[_ValidAuthorityInput] =>
    Generators.map3[
      (String val | None),
      String val,
      (U16 | None),
      _ValidAuthorityInput](
      _userinfo_gen(),
      _host_gen(),
      _port_gen(),
      {(userinfo, host, port) =>
        _ValidAuthorityInput(userinfo, host, port)
      })

  fun _userinfo_gen(): Generator[(String val | None)] =>
    Generators.frequency[(String val | None)](
      [
        as WeightedGenerator[(String val | None)]:
        (2, Generators.unit[(String val | None)](None))
        (1, Generators.one_of[String val](
          ["user"; "user:pass"; "admin"; "user%40example"])
          .map[(String val | None)]({(s) => s }))
      ])

  fun _host_gen(): Generator[String val] =>
    Generators.one_of[String val](
      [
        "example.com"; "localhost"; "192.168.1.1"
        "sub.domain.example.org"; "my-host"
      ])

  fun _port_gen(): Generator[(U16 | None)] =>
    Generators.frequency[(U16 | None)](
      [
        as WeightedGenerator[(U16 | None)]:
        (1, Generators.unit[(U16 | None)](None))
        (1, Generators.one_of[U16]([as U16: 80; 443; 8080; 3000; 8443])
          .map[(U16 | None)]({(p) => p }))
      ])
