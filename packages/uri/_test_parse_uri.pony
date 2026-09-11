use "pony_test"
use "pony_check"

class \nodoc\ iso _PropertyURIRoundtrip is Property1[_ValidURIInput]
  """
  For generated valid URIs, ParseURI(uri.string()) produces an equal URI.
  """
  fun name(): String => "uri/parse_uri/roundtrip"

  fun gen(): Generator[_ValidURIInput] =>
    _ValidURIInputGenerator()

  fun ref property(arg1: _ValidURIInput, ph: PropertyHelper) =>
    let original =
      URI(
        arg1.scheme,
        arg1.authority,
        arg1.path,
        arg1.query,
        arg1.fragment)
    let serialized = original.string()
    match \exhaustive\ ParseURI(consume serialized)
    | let reparsed: URI val =>
      ph.assert_true(
        original == reparsed,
        "roundtrip failed for: " + original.string())
    | let err: URIParseError val =>
      ph.fail("roundtrip parse failed for: " + original.string() +
        " error: " + err.string())
    end

class \nodoc\ iso _PropertyInvalidSchemeRejected is Property1[String val]
  """
  Invalid schemes (starting with digit, containing illegal chars) produce
  InvalidScheme.
  """
  fun name(): String => "uri/parse_uri/invalid_scheme_rejected"

  fun gen(): Generator[String val] =>
    Generators.frequency[String val](
      [
        as WeightedGenerator[String val]:
        // starts with digit
        (1, Generators.ascii_numeric(1, 1)
          .map[String val]({(s) => s + "foo://host" }))
        // contains illegal character
        (1, Generators.one_of[String val](
          ["sch eme://host"; "sch@eme://host"; "sch[eme://host"]))
      ])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    // These should either parse as relative references (no scheme)
    // or produce an error. They should NOT parse with a scheme.
    match \exhaustive\ ParseURI(arg1)
    | let u: URI val =>
      ph.assert_true(
        u.scheme is None,
        "expected no scheme for: " + arg1)
    | let err: URIParseError val =>
      ph.assert_true(true) // error is also acceptable
    end

class \nodoc\ iso _TestParseURIKnownGood is UnitTest
  """
  RFC 3986 section 1.1.2 examples, HTTP request-target forms, and edge cases.
  """
  fun name(): String => "uri/parse_uri/known_good"

  fun ref apply(h: TestHelper) =>
    // RFC 3986 examples
    _assert_uri(
      h,
      "ftp://ftp.is.co.za/rfc/rfc1808.txt",
      "ftp",
      "ftp.is.co.za",
      "/rfc/rfc1808.txt",
      None,
      None)

    _assert_uri(
      h,
      "http://www.ietf.org/rfc/rfc2396.txt",
      "http",
      "www.ietf.org",
      "/rfc/rfc2396.txt",
      None,
      None)

    _assert_uri(
      h,
      "ldap://[2001:db8::7]/c=GB?objectClass?one",
      "ldap",
      "[2001:db8::7]",
      "/c=GB",
      "objectClass?one",
      None)

    _assert_uri(
      h,
      "mailto:John.Doe@example.com",
      "mailto",
      None,
      "John.Doe@example.com",
      None,
      None)

    _assert_uri(
      h,
      "tel:+1-816-555-1212",
      "tel",
      None,
      "+1-816-555-1212",
      None,
      None)

    _assert_uri(
      h,
      "urn:oasis:names:specification:docbook:dtd:xml:4.1.2",
      "urn",
      None,
      "oasis:names:specification:docbook:dtd:xml:4.1.2",
      None,
      None)

    // HTTP request-target forms
    // origin-form
    _assert_uri(
      h,
      "/index.html?page=1",
      None,
      None,
      "/index.html",
      "page=1",
      None)

    // absolute-form
    _assert_uri(
      h,
      "http://www.example.org/pub/WWW/TheProject.html",
      "http",
      "www.example.org",
      "/pub/WWW/TheProject.html",
      None,
      None)

    // asterisk-form
    _assert_uri(
      h, "*", None, None, "*", None, None)

    // Edge cases
    // empty string = valid empty relative reference
    _assert_uri(
      h, "", None, None, "", None, None)

    // query without value
    match \exhaustive\ ParseURI("?key")
    | let u: URI val =>
      h.assert_eq[String val]("", u.path)
      match \exhaustive\ u.query
      | let q: String => h.assert_eq[String val]("key", q)
      else h.fail("expected query for ?key")
      end
    | let e: URIParseError val =>
      h.fail("parse failed for ?key: " + e.string())
    end

    // empty query present (/path?)
    match \exhaustive\ ParseURI("/path?")
    | let u: URI val =>
      h.assert_eq[String val]("/path", u.path)
      match \exhaustive\ u.query
      | let q: String => h.assert_eq[String val]("", q)
      else h.fail("expected empty query for /path?")
      end
    | let e: URIParseError val =>
      h.fail("parse failed for /path?: " + e.string())
    end

    // fragment only
    _assert_uri(
      h, "#frag", None, None, "", None, "frag")

    // authority without port
    _assert_uri(
      h,
      "http://example.com/path",
      "http",
      "example.com",
      "/path",
      None,
      None)

    // empty authority (file:///etc/hosts)
    match \exhaustive\ ParseURI("file:///etc/hosts")
    | let u: URI val =>
      match \exhaustive\ u.scheme
      | let s: String => h.assert_eq[String val]("file", s)
      else h.fail("expected scheme for file:///etc/hosts")
      end
      match \exhaustive\ u.authority
      | let a: URIAuthority =>
        h.assert_eq[String val]("", a.host)
      else h.fail("expected authority for file:///etc/hosts")
      end
      h.assert_eq[String val]("/etc/hosts", u.path)
    | let e: URIParseError val =>
      h.fail("parse failed for file:///etc/hosts: " + e.string())
    end

    // Percent-encoded delimiters not treated as structural
    match \exhaustive\ ParseURI("http://example.com/a%2Fb?c%3Fd")
    | let u: URI val =>
      h.assert_eq[String val]("/a%2Fb", u.path)
      match \exhaustive\ u.query
      | let q: String => h.assert_eq[String val]("c%3Fd", q)
      else h.fail("expected query for percent-encoded test")
      end
    | let e: URIParseError val =>
      h.fail("parse failed for percent-encoded: " + e.string())
    end

    // Authority with port
    _assert_uri(
      h,
      "http://example.com:8080/path",
      "http",
      "example.com:8080",
      "/path",
      None,
      None)

    // Authority with userinfo
    match \exhaustive\ ParseURI("http://user:pass@example.com/path")
    | let u: URI val =>
      match u.authority
      | let a: URIAuthority =>
        match a.userinfo
        | let ui: String => h.assert_eq[String val]("user:pass", ui)
        else h.fail("expected userinfo")
        end
        h.assert_eq[String val]("example.com", a.host)
      else h.fail("expected authority")
      end
    | let e: URIParseError val =>
      h.fail("parse failed for userinfo: " + e.string())
    end

    // Full URI with all components
    let full_uri = "http://user@example.com:8080/path?query=1#frag"
    match \exhaustive\ ParseURI(full_uri)
    | let u: URI val =>
      match u.scheme
      | let s: String => h.assert_eq[String val]("http", s)
      else h.fail("expected scheme")
      end
      match u.authority
      | let a: URIAuthority =>
        match a.userinfo
        | let ui: String => h.assert_eq[String val]("user", ui)
        else h.fail("expected userinfo")
        end
        h.assert_eq[String val]("example.com", a.host)
        match a.port
        | let p: U16 => h.assert_eq[U16](8080, p)
        else h.fail("expected port")
        end
      else h.fail("expected authority")
      end
      h.assert_eq[String val]("/path", u.path)
      match u.query
      | let q: String => h.assert_eq[String val]("query=1", q)
      else h.fail("expected query")
      end
      match u.fragment
      | let f: String => h.assert_eq[String val]("frag", f)
      else h.fail("expected fragment")
      end
    | let e: URIParseError val =>
      h.fail("parse failed for full URI: " + e.string())
    end

  fun _assert_uri(
    h: TestHelper,
    input: String val,
    expected_scheme: (String | None),
    expected_authority: (String | None),
    expected_path: String,
    expected_query: (String | None),
    expected_fragment: (String | None))
  =>
    match \exhaustive\ ParseURI(input)
    | let u: URI val =>
      match \exhaustive\ (expected_scheme, u.scheme)
      | (None, None) => None
      | (let e: String, let a: String) =>
        h.assert_eq[String val](e, a, "scheme mismatch for: " + input)
      else
        h.fail("scheme mismatch for: " + input)
      end

      match \exhaustive\ (expected_authority, u.authority)
      | (None, None) => None
      | (let e: String, let a: URIAuthority) =>
        h.assert_eq[String val](
          e, a.string(), "authority mismatch for: " + input)
      else
        h.fail("authority mismatch for: " + input)
      end

      h.assert_eq[String val](
        expected_path, u.path, "path mismatch for: " + input)

      match \exhaustive\ (expected_query, u.query)
      | (None, None) => None
      | (let e: String, let a: String) =>
        h.assert_eq[String val](e, a, "query mismatch for: " + input)
      else
        h.fail("query mismatch for: " + input)
      end

      match \exhaustive\ (expected_fragment, u.fragment)
      | (None, None) => None
      | (let e: String, let a: String) =>
        h.assert_eq[String val](e, a, "fragment mismatch for: " + input)
      else
        h.fail("fragment mismatch for: " + input)
      end
    | let err: URIParseError val =>
      h.fail("parse failed for " + input + ": " + err.string())
    end

// -- Generators --
class \nodoc\ val _ValidURIInput
  let scheme: (String | None)
  let authority: (URIAuthority | None)
  let path: String
  let query: (String | None)
  let fragment: (String | None)

  new val create(
    scheme': (String | None),
    authority': (URIAuthority | None),
    path': String,
    query': (String | None),
    fragment': (String | None))
  =>
    scheme = scheme'
    authority = authority'
    path = path'
    query = query'
    fragment = fragment'

primitive \nodoc\ _ValidURIInputGenerator
  fun apply(): Generator[_ValidURIInput] =>
    // PonyCheck has map4 max, so we combine scheme+authority into one
    // generator via map2, then compose with path, query, fragment.
    Generators.map4[
      (String val | None, URIAuthority val | None),
      String val,
      (String val | None),
      (String val | None),
      _ValidURIInput](
      _scheme_authority_gen(),
      _path_gen(),
      _query_gen(),
      _fragment_gen(),
      {(scheme_auth, path, query, fragment) =>
        (let scheme, let authority) = scheme_auth
        _ValidURIInput(scheme, authority, path, query, fragment)
      })

  fun _scheme_authority_gen()
    : Generator[(String val | None, URIAuthority val | None)]
  =>
    Generators.map2[
      (String val | None),
      (URIAuthority val | None),
      (String val | None, URIAuthority val | None)](
      _scheme_gen(),
      _authority_gen(),
      {(scheme, authority) =>
        // Authority requires a scheme (absolute URI) to roundtrip correctly
        // through ParseURI — a relative reference with "//" would be
        // ambiguous without scheme context.
        match scheme
        | let _: String => (scheme, authority)
        else (scheme, None)
        end
      })

  fun _scheme_gen(): Generator[(String val | None)] =>
    Generators.frequency[(String val | None)](
      [
        as WeightedGenerator[(String val | None)]:
        (1, Generators.unit[(String val | None)](None))
        (2, Generators.one_of[String val](
          ["http"; "https"; "ftp"; "ssh"])
          .map[(String val | None)]({(s) => s }))
      ])

  fun _authority_gen(): Generator[(URIAuthority val | None)] =>
    Generators.frequency[(URIAuthority val | None)](
      [
        as WeightedGenerator[(URIAuthority val | None)]:
        (1, Generators.unit[(URIAuthority val | None)](None))
        (1, Generators.one_of[String val](
          [ "example.com"; "localhost"; "192.168.1.1"; "example.com:8080"
            "example.com:443"])
          .map[(URIAuthority val | None)]({(host) =>
            match ParseURIAuthority(host)
            | let a: URIAuthority val => a
            else
              // Generator strings are all valid; fallback to simple host
              URIAuthority(None, "localhost", None)
            end
          }))
      ])

  fun _path_gen(): Generator[String val] =>
    Generators.frequency[String val](
      [
        as WeightedGenerator[String val]:
        (1, Generators.unit[String val](""))
        (1, Generators.unit[String val]("/"))
        (2, Generators.one_of[String val](
          [ "/path"; "/a/b/c"; "/index.html"; "/foo/bar/baz"
            "/path/to/resource"]))
      ])

  fun _query_gen(): Generator[(String val | None)] =>
    Generators.frequency[(String val | None)](
      [
        as WeightedGenerator[(String val | None)]:
        (2, Generators.unit[(String val | None)](None))
        (1, Generators.one_of[String val](
          ["key=value"; "a=1&b=2"; "q=hello+world"; ""; "page=1"])
          .map[(String val | None)]({(s) => s }))
      ])

  fun _fragment_gen(): Generator[(String val | None)] =>
    Generators.frequency[(String val | None)](
      [
        as WeightedGenerator[(String val | None)]:
        (2, Generators.unit[(String val | None)](None))
        (1, Generators.one_of[String val](
          ["top"; "section1"; ""; "frag"])
          .map[(String val | None)]({(s) => s }))
      ])
