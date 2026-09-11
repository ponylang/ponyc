use "pony_test"
use "pony_check"

// -- Example-based tests --

class \nodoc\ iso _TestBuildSimple is UnitTest
  """scheme + host + path produces expected URI string."""
  fun name(): String => "uri/builder/simple"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ URIBuilder
      .set_scheme("https")
      .set_host("example.com")
      .set_path("/index.html")
      .build()
    | let u: URI val =>
      h.assert_eq[String val]("https://example.com/index.html", u.string())
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

class \nodoc\ iso _TestBuildAllComponents is UnitTest
  """All components set produces correct URI string."""
  fun name(): String => "uri/builder/all_components"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ URIBuilder
      .set_scheme("http")
      .set_userinfo("user:pass")
      .set_host("example.com")
      .set_port(8080)
      .set_path("/path")
      .set_query("key=value")
      .set_fragment("frag")
      .build()
    | let u: URI val =>
      h.assert_eq[String val](
        "http://user:pass@example.com:8080/path?key=value#frag",
        u.string())
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

class \nodoc\ iso _TestBuildQueryParams is UnitTest
  """add_query_param produces correct query string."""
  fun name(): String => "uri/builder/query_params"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ URIBuilder
      .set_scheme("https")
      .set_host("example.com")
      .add_query_param("a", "1")
      .add_query_param("b", "2")
      .build()
    | let u: URI val =>
      h.assert_eq[String val](
        "https://example.com?a=1&b=2", u.string())
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

class \nodoc\ iso _TestBuildQueryParamEncoding is UnitTest
  """Keys/values with =, &, +, and spaces are properly encoded."""
  fun name(): String => "uri/builder/query_param_encoding"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ URIBuilder
      .set_scheme("https")
      .set_host("example.com")
      .add_query_param("a=b", "c&d")
      .add_query_param("e+f", "g h")
      .build()
    | let u: URI val =>
      match u.query
      | let q: String =>
        h.assert_eq[String val](
          "a%3Db=c%26d&e%2Bf=g%20h", q)
      else
        h.fail("expected query")
      end
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

class \nodoc\ iso _TestBuildFromURI is UnitTest
  """from(existing_uri) reproduces the original URI string."""
  fun name(): String => "uri/builder/from_uri"

  fun ref apply(h: TestHelper) =>
    let full = "http://user@example.com:8080/path?query=1#frag"
    match \exhaustive\ ParseURI(full)
    | let original: URI val =>
      match \exhaustive\ URIBuilder.from(original).build()
      | let rebuilt: URI val =>
        h.assert_eq[String val](original.string(), rebuilt.string())
      | let e: URIBuildError val =>
        h.fail("build failed: " + e.string())
      end
    | let e: URIParseError val =>
      h.fail("parse failed: " + e.string())
    end

class \nodoc\ iso _TestBuildModifyFromURI is UnitTest
  """from(uri) then change host and add query param."""
  fun name(): String => "uri/builder/modify_from_uri"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ ParseURI("https://example.com/path?x=1")
    | let original: URI val =>
      match \exhaustive\ URIBuilder.from(original)
        .set_host("other.com")
        .add_query_param("y", "2")
        .build()
      | let modified: URI val =>
        h.assert_eq[String val](
          "https://other.com/path?x=1&y=2", modified.string())
      | let e: URIBuildError val =>
        h.fail("build failed: " + e.string())
      end
    | let e: URIParseError val =>
      h.fail("parse failed: " + e.string())
    end

class \nodoc\ iso _TestBuildIPLiteralHost is UnitTest
  """IPv6 host [::1] is stored and validated correctly."""
  fun name(): String => "uri/builder/ip_literal_host"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ URIBuilder
      .set_scheme("http")
      .set_host("[::1]")
      .set_port(8080)
      .build()
    | let u: URI val =>
      h.assert_eq[String val]("http://[::1]:8080", u.string())
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

class \nodoc\ iso _TestBuildAutoEncode is UnitTest
  """Spaces and special chars in path/query/fragment are encoded."""
  fun name(): String => "uri/builder/auto_encode"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ URIBuilder
      .set_scheme("https")
      .set_host("example.com")
      .set_path("/hello world")
      .set_query("key=val ue")
      .set_fragment("sec tion")
      .build()
    | let u: URI val =>
      h.assert_eq[String val](
        "https://example.com/hello%20world?key=val%20ue#sec%20tion",
        u.string())
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

class \nodoc\ iso _TestBuildPathAutoSlash is UnitTest
  """Authority present + relative path gets / prepended."""
  fun name(): String => "uri/builder/path_auto_slash"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ URIBuilder
      .set_scheme("http")
      .set_host("example.com")
      .set_path("relative")
      .build()
    | let u: URI val =>
      h.assert_eq[String val](
        "http://example.com/relative", u.string())
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

class \nodoc\ iso _TestBuildAppendPathSegment is UnitTest
  """append_path_segment encodes / within segment."""
  fun name(): String => "uri/builder/append_path_segment"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ URIBuilder
      .set_scheme("https")
      .set_host("example.com")
      .append_path_segment("api")
      .append_path_segment("v1")
      .append_path_segment("a/b")
      .build()
    | let u: URI val =>
      h.assert_eq[String val](
        "https://example.com/api/v1/a%2Fb", u.string())
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

class \nodoc\ iso _TestBuildInvalidScheme is UnitTest
  """Invalid scheme chars produce InvalidScheme error."""
  fun name(): String => "uri/builder/invalid_scheme"

  fun ref apply(h: TestHelper) =>
    // starts with digit
    match \exhaustive\ URIBuilder.set_scheme("1http").build()
    | let _: URI val => h.fail("expected error for digit-starting scheme")
    | let e: URIBuildError val =>
      h.assert_true(
        e is InvalidScheme,
        "expected InvalidScheme, got: " + e.string())
    end

    // contains space
    match \exhaustive\ URIBuilder.set_scheme("ht tp").build()
    | let _: URI val => h.fail("expected error for space in scheme")
    | let e: URIBuildError val =>
      h.assert_true(
        e is InvalidScheme,
        "expected InvalidScheme, got: " + e.string())
    end

    // empty scheme
    match \exhaustive\ URIBuilder.set_scheme("").build()
    | let _: URI val => h.fail("expected error for empty scheme")
    | let e: URIBuildError val =>
      h.assert_true(
        e is InvalidScheme,
        "expected InvalidScheme, got: " + e.string())
    end

class \nodoc\ iso _TestBuildInvalidIPLiteral is UnitTest
  """Malformed IP-literal produces InvalidHost error."""
  fun name(): String => "uri/builder/invalid_ip_literal"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ URIBuilder
      .set_scheme("http")
      .set_host("[not valid")
      .build()
    | let _: URI val => h.fail("expected error for malformed IP-literal")
    | let e: URIBuildError val =>
      h.assert_true(
        e is InvalidHost,
        "expected InvalidHost, got: " + e.string())
    end

class \nodoc\ iso _TestBuildEmpty is UnitTest
  """Empty builder produces empty relative reference."""
  fun name(): String => "uri/builder/empty"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ URIBuilder.build()
    | let u: URI val =>
      h.assert_eq[String val]("", u.string())
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

class \nodoc\ iso _TestBuildQueryNoneVsEmpty is UnitTest
  """None (no ?) vs "" (trailing ? with no value)."""
  fun name(): String => "uri/builder/query_none_vs_empty"

  fun ref apply(h: TestHelper) =>
    // No query — no ? in output
    match \exhaustive\ URIBuilder.set_path("/path").build()
    | let u: URI val =>
      h.assert_eq[String val]("/path", u.string())
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

    // Empty query — trailing ?
    match \exhaustive\ URIBuilder
      .set_path("/path")
      .set_query("")
      .build()
    | let u: URI val =>
      h.assert_eq[String val]("/path?", u.string())
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

class \nodoc\ iso _TestBuildFragmentNoneVsEmpty is UnitTest
  """None (no #) vs "" (trailing # with no value)."""
  fun name(): String => "uri/builder/fragment_none_vs_empty"

  fun ref apply(h: TestHelper) =>
    // No fragment — no # in output
    match \exhaustive\ URIBuilder.set_path("/path").build()
    | let u: URI val =>
      h.assert_eq[String val]("/path", u.string())
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

    // Empty fragment — trailing #
    match \exhaustive\ URIBuilder
      .set_path("/path")
      .set_fragment("")
      .build()
    | let u: URI val =>
      h.assert_eq[String val]("/path#", u.string())
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

class \nodoc\ iso _TestBuildClearMethods is UnitTest
  """clear_* methods reset components."""
  fun name(): String => "uri/builder/clear_methods"

  fun ref apply(h: TestHelper) =>
    let builder = URIBuilder
      .set_scheme("https")
      .set_host("example.com")
      .set_port(443)
      .set_path("/path")
      .set_query("q=1")
      .set_fragment("top")

    // Clear everything
    builder
      .clear_scheme()
      .clear_host()  // also clears userinfo and port
      .set_path("")
      .clear_query()
      .clear_fragment()

    match \exhaustive\ builder.build()
    | let u: URI val =>
      h.assert_eq[String val]("", u.string())
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

class \nodoc\ iso _TestBuildUserinfoAutoAuthority is UnitTest
  """set_userinfo auto-creates empty host if no host set."""
  fun name(): String => "uri/builder/userinfo_auto_authority"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ URIBuilder
      .set_scheme("http")
      .set_userinfo("user")
      .build()
    | let u: URI val =>
      h.assert_eq[String val]("http://user@", u.string())
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

class \nodoc\ iso _TestBuildPortAutoAuthority is UnitTest
  """set_port auto-creates empty host if no host set."""
  fun name(): String => "uri/builder/port_auto_authority"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ URIBuilder
      .set_scheme("http")
      .set_port(8080)
      .build()
    | let u: URI val =>
      h.assert_eq[String val]("http://:8080", u.string())
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

class \nodoc\ iso _TestBuildQuerySetThenAdd is UnitTest
  """set_query("a=1") then add_query_param("b", "2") appends correctly."""
  fun name(): String => "uri/builder/query_set_then_add"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ URIBuilder
      .set_scheme("https")
      .set_host("example.com")
      .set_query("a=1")
      .add_query_param("b", "2")
      .build()
    | let u: URI val =>
      match u.query
      | let q: String =>
        h.assert_eq[String val]("a=1&b=2", q)
      else
        h.fail("expected query")
      end
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

class \nodoc\ iso _TestBuildAuthorityNoScheme is UnitTest
  """Host + path without scheme produces //host/path."""
  fun name(): String => "uri/builder/authority_no_scheme"

  fun ref apply(h: TestHelper) =>
    match \exhaustive\ URIBuilder
      .set_host("example.com")
      .set_path("/path")
      .build()
    | let u: URI val =>
      h.assert_eq[String val]("//example.com/path", u.string())
    | let e: URIBuildError val =>
      h.fail("build failed: " + e.string())
    end

// -- Property-based tests --
class \nodoc\ iso _PropertyBuildFromRoundtrip is Property1[_ValidURIInput]
  """
  For generated valid URIs, URIBuilder.from(uri).build() produces a URI
  that string-equals the original.
  """
  fun name(): String => "uri/builder/from_roundtrip"

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
    let original_str = original.string()
    match \exhaustive\ URIBuilder.from(original).build()
    | let rebuilt: URI val =>
      ph.assert_eq[String val](
        consume original_str,
        rebuilt.string(),
        "roundtrip failed for: " + original.string())
    | let e: URIBuildError val =>
      ph.fail("build failed for: " + original.string() +
        " error: " + e.string())
    end

class \nodoc\ iso _PropertyBuildParseRoundtrip is Property1[_BuildInput]
  """
  For generated raw components, build a URI then parse it back —
  components match.
  """
  fun name(): String => "uri/builder/parse_roundtrip"

  fun gen(): Generator[_BuildInput] =>
    _BuildInputGenerator()

  fun ref property(arg1: _BuildInput, ph: PropertyHelper) =>
    let builder = URIBuilder
      .set_scheme(arg1.scheme)
      .set_host(arg1.host)
      .set_path(arg1.path)

    match \exhaustive\ builder.build()
    | let built: URI val =>
      let s = built.string()
      match \exhaustive\ ParseURI(consume s)
      | let parsed: URI val =>
        match parsed.scheme
        | let s': String =>
          ph.assert_eq[String val](
            arg1.scheme, s', "scheme mismatch")
        else
          ph.fail("expected scheme")
        end
        match parsed.authority
        | let a: URIAuthority =>
          ph.assert_eq[String val](
            PercentEncode(arg1.host, URIPartHost),
            a.host,
            "host mismatch")
        else
          ph.fail("expected authority")
        end
      | let e: URIParseError val =>
        ph.fail("parse failed: " + e.string())
      end
    | let e: URIBuildError val =>
      ph.fail("build failed: " + e.string())
    end

class \nodoc\ iso _PropertyBuildInvalidSchemeFails
  is Property1[String val]
  """
  Generated invalid scheme strings always produce InvalidScheme on build().
  """
  fun name(): String => "uri/builder/invalid_scheme_fails"

  fun gen(): Generator[String val] =>
    _InvalidSchemeGenerator()

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    match \exhaustive\ URIBuilder.set_scheme(arg1).build()
    | let _: URI val =>
      ph.fail("expected InvalidScheme for: " + arg1)
    | let e: URIBuildError val =>
      ph.assert_true(
        e is InvalidScheme,
        "expected InvalidScheme for: " + arg1 +
          " got: " + e.string())
    end

// -- Generators --
class \nodoc\ val _BuildInput
  let scheme: String
  let host: String
  let path: String

  new val create(scheme': String, host': String, path': String) =>
    scheme = scheme'
    host = host'
    path = path'

primitive \nodoc\ _BuildInputGenerator
  fun apply(): Generator[_BuildInput] =>
    Generators.map3[String val, String val, String val, _BuildInput](
      Generators.one_of[String val](
        ["http"; "https"; "ftp"; "ssh"]),
      Generators.one_of[String val](
        ["example.com"; "localhost"; "myhost"]),
      Generators.one_of[String val](
        ["/path"; "/a/b/c"; "/index.html"; "/"]),
      {(scheme, host, path) => _BuildInput(scheme, host, path) })

primitive \nodoc\ _InvalidSchemeGenerator
  fun apply(): Generator[String val] =>
    Generators.frequency[String val](
      [
        as WeightedGenerator[String val]:
        // starts with digit
        (1, Generators.one_of[String val](
          ["1http"; "9scheme"; "0abc"]))
        // contains space
        (1, Generators.one_of[String val](
          ["ht tp"; "a scheme"; "my uri"]))
        // contains @ or [
        (1, Generators.one_of[String val](
          ["sch@eme"; "sch[eme"; "a]b"]))
        // empty string
        (1, Generators.unit[String val](""))
      ])
