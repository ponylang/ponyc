use "pony_test"
use "pony_check"

class \nodoc\ iso _PropertyResolveResultAbsolute
  is Property1[_ResolveInput]
  """
  For any absolute base and any reference, the resolved URI always has a
  scheme.
  """
  fun name(): String => "uri/resolve_uri/result_always_absolute"

  fun gen(): Generator[_ResolveInput] =>
    _ResolveInputGenerator()

  fun ref property(arg1: _ResolveInput, ph: PropertyHelper) =>
    let base =
      URI(
        arg1.base.scheme,
        arg1.base.authority,
        arg1.base.path,
        arg1.base.query,
        arg1.base.fragment)
    let reference =
      URI(
        arg1.reference.scheme,
        arg1.reference.authority,
        arg1.reference.path,
        arg1.reference.query,
        arg1.reference.fragment)
    match \exhaustive\ ResolveURI(base, reference)
    | let result: URI val =>
      ph.assert_true(
        match result.scheme
        | let _: String => true
        else false
        end,
        "result has no scheme for base: " + base.string() +
          " ref: " + reference.string())
    | let e: ResolveURIError val =>
      ph.fail("unexpected error: " + e.string())
    end

class \nodoc\ iso _PropertyResolveEmptyRef
  is Property1[_AbsoluteURIInput]
  """
  Resolving an empty reference against a base returns the base URI with the
  fragment dropped (fragment always comes from the reference, and an empty
  reference has no fragment).
  """
  fun name(): String => "uri/resolve_uri/empty_ref_returns_base"

  fun gen(): Generator[_AbsoluteURIInput] =>
    _AbsoluteURIInputGenerator()

  fun ref property(arg1: _AbsoluteURIInput, ph: PropertyHelper) =>
    let base =
      URI(
        arg1.scheme,
        arg1.authority,
        arg1.path,
        arg1.query,
        arg1.fragment)
    let empty_ref = URI(None, None, "", None, None)
    match \exhaustive\ ResolveURI(base, empty_ref)
    | let result: URI val =>
      let expected =
        URI(
          arg1.scheme,
          arg1.authority,
          arg1.path,
          arg1.query,
          None)
      ph.assert_true(
        result == expected,
        "expected " + expected.string() + " got " + result.string())
    | let e: ResolveURIError val =>
      ph.fail("unexpected error: " + e.string())
    end

class \nodoc\ iso _PropertyAbsoluteRefIgnoresBase
  is Property1[(_AbsoluteURIInput, _AbsoluteURIInput)]
  """
  When the reference has a scheme, the result's scheme matches the
  reference's scheme regardless of the base.
  """
  fun name(): String => "uri/resolve_uri/absolute_ref_ignores_base"

  fun gen(): Generator[(_AbsoluteURIInput, _AbsoluteURIInput)] =>
    Generators.zip2[_AbsoluteURIInput, _AbsoluteURIInput](
      _AbsoluteURIInputGenerator(),
      _AbsoluteURIInputGenerator())

  fun ref property(
    arg1: (_AbsoluteURIInput, _AbsoluteURIInput),
    ph: PropertyHelper)
  =>
    (let base_in, let ref_in) = arg1
    let base =
      URI(
        base_in.scheme,
        base_in.authority,
        base_in.path,
        base_in.query,
        base_in.fragment)
    let reference =
      URI(
        ref_in.scheme,
        ref_in.authority,
        ref_in.path,
        ref_in.query,
        ref_in.fragment)
    match \exhaustive\ ResolveURI(base, reference)
    | let result: URI val =>
      match result.scheme
      | let s: String =>
        ph.assert_eq[String val](ref_in.scheme, s)
      else
        ph.fail("result has no scheme")
      end
    | let e: ResolveURIError val =>
      ph.fail("unexpected error: " + e.string())
    end

class \nodoc\ iso _PropertyNonAbsoluteBaseRejected
  is Property1[_ValidURIInput]
  """
  A base URI without a scheme always produces BaseURINotAbsolute, regardless
  of the reference.
  """
  fun name(): String => "uri/resolve_uri/non_absolute_base_rejected"

  fun gen(): Generator[_ValidURIInput] =>
    _ValidURIInputGenerator()

  fun ref property(arg1: _ValidURIInput, ph: PropertyHelper) =>
    let base = URI(None, None, "/some/path", "query", "frag")
    let reference =
      URI(
        arg1.scheme,
        arg1.authority,
        arg1.path,
        arg1.query,
        arg1.fragment)
    match \exhaustive\ ResolveURI(base, reference)
    | let _: URI val =>
      ph.fail("expected BaseURINotAbsolute")
    | let _: BaseURINotAbsolute => ph.assert_true(true)
    end

class \nodoc\ iso _PropertyResolveRoundtrip
  is Property1[_ResolveInput]
  """
  The resolved URI roundtrips through string() and ParseURI: parsing the
  string form produces an equal URI.
  """
  fun name(): String => "uri/resolve_uri/roundtrip"

  fun gen(): Generator[_ResolveInput] =>
    _ResolveInputGenerator()

  fun ref property(arg1: _ResolveInput, ph: PropertyHelper) =>
    let base =
      URI(
        arg1.base.scheme,
        arg1.base.authority,
        arg1.base.path,
        arg1.base.query,
        arg1.base.fragment)
    let reference =
      URI(
        arg1.reference.scheme,
        arg1.reference.authority,
        arg1.reference.path,
        arg1.reference.query,
        arg1.reference.fragment)
    match \exhaustive\ ResolveURI(base, reference)
    | let result: URI val =>
      match \exhaustive\ ParseURI(result.string())
      | let reparsed: URI val =>
        ph.assert_true(
          result == reparsed,
          "roundtrip failed for: " + result.string())
      | let e: URIParseError val =>
        ph.fail("reparse failed for: " + result.string() +
          " error: " + e.string())
      end
    | let e: ResolveURIError val =>
      ph.fail("unexpected error: " + e.string())
    end

class \nodoc\ iso _TestResolveURIRFCNormal is UnitTest
  """
  RFC 3986 section 5.4.1 — normal resolution examples.
  Base: http://a/b/c/d;p?q
  """
  fun name(): String => "uri/resolve_uri/rfc_normal"

  fun ref apply(h: TestHelper) =>
    let base = "http://a/b/c/d;p?q"
    _AssertResolve(h, base, "g:h",      "g:h")
    _AssertResolve(h, base, "g",        "http://a/b/c/g")
    _AssertResolve(h, base, "./g",      "http://a/b/c/g")
    _AssertResolve(h, base, "g/",       "http://a/b/c/g/")
    _AssertResolve(h, base, "/g",       "http://a/g")
    _AssertResolve(h, base, "//g",      "http://g")
    _AssertResolve(h, base, "?y",       "http://a/b/c/d;p?y")
    _AssertResolve(h, base, "g?y",      "http://a/b/c/g?y")
    _AssertResolve(h, base, "#s",       "http://a/b/c/d;p?q#s")
    _AssertResolve(h, base, "g#s",      "http://a/b/c/g#s")
    _AssertResolve(h, base, "g?y#s",    "http://a/b/c/g?y#s")
    _AssertResolve(h, base, ";x",       "http://a/b/c/;x")
    _AssertResolve(h, base, "g;x",      "http://a/b/c/g;x")
    _AssertResolve(h, base, "g;x?y#s",  "http://a/b/c/g;x?y#s")
    _AssertResolve(h, base, "",         "http://a/b/c/d;p?q")
    _AssertResolve(h, base, ".",        "http://a/b/c/")
    _AssertResolve(h, base, "./",       "http://a/b/c/")
    _AssertResolve(h, base, "..",       "http://a/b/")
    _AssertResolve(h, base, "../",      "http://a/b/")
    _AssertResolve(h, base, "../g",     "http://a/b/g")
    _AssertResolve(h, base, "../..",    "http://a/")
    _AssertResolve(h, base, "../../",   "http://a/")
    _AssertResolve(h, base, "../../g",  "http://a/g")

class \nodoc\ iso _TestResolveURIRFCAbnormal is UnitTest
  """
  RFC 3986 section 5.4.2 — abnormal resolution examples.
  Base: http://a/b/c/d;p?q
  """
  fun name(): String => "uri/resolve_uri/rfc_abnormal"

  fun ref apply(h: TestHelper) =>
    let base = "http://a/b/c/d;p?q"
    // Excess ".." — clamps to root
    _AssertResolve(h, base, "../../../g",    "http://a/g")
    _AssertResolve(h, base, "../../../../g", "http://a/g")

    // Absolute paths with dots
    _AssertResolve(h, base, "/./g",  "http://a/g")
    _AssertResolve(h, base, "/../g", "http://a/g")

    // Compound and unnecessary dot-segment forms
    _AssertResolve(h, base, "./../g", "http://a/b/g")
    _AssertResolve(h, base, "./g.",   "http://a/b/c/g.")
    _AssertResolve(h, base, "./g/.",  "http://a/b/c/g/")
    _AssertResolve(h, base, "g/./h",  "http://a/b/c/g/h")
    _AssertResolve(h, base, "g/../h", "http://a/b/c/h")

    // Not dot segments (contain extra characters)
    _AssertResolve(h, base, "g.",  "http://a/b/c/g.")
    _AssertResolve(h, base, "g..", "http://a/b/c/g..")
    _AssertResolve(h, base, ".g",  "http://a/b/c/.g")
    _AssertResolve(h, base, "..g", "http://a/b/c/..g")

    // Dot segments in mid-path
    _AssertResolve(h, base, "g;x=1/./y",  "http://a/b/c/g;x=1/y")
    _AssertResolve(h, base, "g;x=1/../y", "http://a/b/c/y")

    // Dots in query and fragment (not path — left as-is)
    _AssertResolve(h, base, "g?y/./x",  "http://a/b/c/g?y/./x")
    _AssertResolve(h, base, "g?y/../x", "http://a/b/c/g?y/../x")
    _AssertResolve(h, base, "g#s/./x",  "http://a/b/c/g#s/./x")
    _AssertResolve(h, base, "g#s/../x", "http://a/b/c/g#s/../x")

class \nodoc\ iso _TestResolveURIEdgeCases is UnitTest
  """
  Additional edge cases beyond the RFC 3986 section 5.4 suite.
  """
  fun name(): String => "uri/resolve_uri/edge_cases"

  fun ref apply(h: TestHelper) =>
    // Base with empty path + authority
    _AssertResolve(
      h, "http://example.com", "g", "http://example.com/g")

    // Reference with authority
    _AssertResolve(
      h,
      "http://base.com/path",
      "//other.com/path",
      "http://other.com/path")

    // Fragment-only reference
    _AssertResolve(
      h,
      "http://example.com/path?q",
      "#frag",
      "http://example.com/path?q#frag")

    // Query-only reference
    _AssertResolve(
      h,
      "http://example.com/path?old",
      "?new",
      "http://example.com/path?new")

    // Base with userinfo and port preserved through resolution.
    // From /a/b, "../c" goes up from directory /a/ to /, then to /c.
    _AssertResolve(
      h,
      "http://user@example.com:8080/a/b",
      "../c",
      "http://user@example.com:8080/c")

    // Non-absolute base rejected
    match \exhaustive\ ParseURI("/relative/path")
    | let base: URI val =>
      match \exhaustive\ ParseURI("g")
      | let ref': URI val =>
        match \exhaustive\ ResolveURI(base, ref')
        | let _: URI val =>
          h.fail("expected BaseURINotAbsolute for relative base")
        | let _: BaseURINotAbsolute => None
        end
      | let e: URIParseError val =>
        h.fail("parse error: " + e.string())
      end
    | let e: URIParseError val =>
      h.fail("parse error: " + e.string())
    end

primitive \nodoc\ _AssertResolve
  fun apply(
    h: TestHelper,
    base_str: String,
    reference_str: String,
    expected: String)
  =>
    match \exhaustive\ ParseURI(base_str)
    | let base: URI val =>
      match \exhaustive\ ParseURI(reference_str)
      | let reference: URI val =>
        match \exhaustive\ ResolveURI(base, reference)
        | let result: URI val =>
          h.assert_eq[String val](
            expected,
            result.string(),
            "resolve(" + base_str + ", " + reference_str + ")")
        | let e: ResolveURIError val =>
          h.fail("resolve error for (" + base_str + ", " + reference_str +
            "): " + e.string())
        end
      | let e: URIParseError val =>
        h.fail("parse error for reference " + reference_str +
          ": " + e.string())
      end
    | let e: URIParseError val =>
      h.fail("parse error for base " + base_str + ": " + e.string())
    end

// -- Generators --
class \nodoc\ val _AbsoluteURIInput
  let scheme: String
  let authority: (URIAuthority | None)
  let path: String
  let query: (String | None)
  let fragment: (String | None)

  new val create(
    scheme': String,
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

primitive \nodoc\ _AbsoluteURIInputGenerator
  fun apply(): Generator[_AbsoluteURIInput] =>
    Generators.map4[
      (String val, URIAuthority val | None),
      String val,
      (String val | None),
      (String val | None),
      _AbsoluteURIInput](
      _scheme_authority_gen(),
      _path_gen(),
      _query_gen(),
      _fragment_gen(),
      {(scheme_auth, path, query, fragment) =>
        (let scheme, let authority) = scheme_auth
        _AbsoluteURIInput(scheme, authority, path, query, fragment)
      })

  fun _scheme_authority_gen()
    : Generator[(String val, URIAuthority val | None)]
  =>
    Generators.map2[
      String val,
      (URIAuthority val | None),
      (String val, URIAuthority val | None)](
      _scheme_gen(),
      _authority_gen(),
      {(scheme, authority) => (scheme, authority) })

  fun _scheme_gen(): Generator[String val] =>
    Generators.one_of[String val](
      ["http"; "https"; "ftp"; "ssh"])

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

class \nodoc\ val _ResolveInput
  let base: _AbsoluteURIInput
  let reference: _ValidURIInput

  new val create(
    base': _AbsoluteURIInput,
    reference': _ValidURIInput)
  =>
    base = base'
    reference = reference'

primitive \nodoc\ _ResolveInputGenerator
  fun apply(): Generator[_ResolveInput] =>
    Generators.map2[_AbsoluteURIInput, _ValidURIInput, _ResolveInput](
      _AbsoluteURIInputGenerator(),
      _ValidURIInputGenerator(),
      {(base, reference) => _ResolveInput(base, reference) })
