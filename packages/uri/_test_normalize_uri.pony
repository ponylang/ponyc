use "pony_test"
use "pony_check"

// -- Property-based tests --

class \nodoc\ iso _PropertyNormalizeIdempotent
  is Property1[_NormalizableURIInput]
  """
  Normalizing an already-normalized URI produces the same URI.
  """
  fun name(): String => "uri/normalize_uri/idempotent"

  fun gen(): Generator[_NormalizableURIInput] =>
    _NormalizableURIInputGenerator()

  fun ref property(arg1: _NormalizableURIInput, ph: PropertyHelper) =>
    let uri = arg1.uri
    match \exhaustive\ NormalizeURI(uri)
    | let once: URI val =>
      match \exhaustive\ NormalizeURI(once)
      | let twice: URI val =>
        ph.assert_true(
          once == twice,
          "not idempotent: once=" + once.string() +
            " twice=" + twice.string() +
            " original=" + uri.string())
      | let e: InvalidPercentEncoding val =>
        ph.fail("second normalization failed for: " + once.string())
      end
    | let e: InvalidPercentEncoding val =>
      ph.fail("first normalization failed for: " + uri.string())
    end

class \nodoc\ iso _PropertyNormalizeSchemeLowercase
  is Property1[_NormalizableURIInput]
  """
  After normalization, the scheme contains no uppercase ASCII.
  """
  fun name(): String => "uri/normalize_uri/scheme_lowercase"

  fun gen(): Generator[_NormalizableURIInput] =>
    _NormalizableURIInputGenerator()

  fun ref property(arg1: _NormalizableURIInput, ph: PropertyHelper) =>
    match \exhaustive\ NormalizeURI(arg1.uri)
    | let u: URI val =>
      match u.scheme
      | let s: String =>
        ph.assert_true(
          not _HasUpperASCII(s),
          "scheme has uppercase: " + s)
      end
    | let e: InvalidPercentEncoding val =>
      ph.fail("normalization failed: " + arg1.uri.string())
    end

class \nodoc\ iso _PropertyNormalizeHostLowercase
  is Property1[_NormalizableURIInput]
  """
  After normalization, non-percent-encoded characters in host have no
  uppercase ASCII.
  """
  fun name(): String => "uri/normalize_uri/host_lowercase"

  fun gen(): Generator[_NormalizableURIInput] =>
    _NormalizableURIInputGenerator()

  fun ref property(arg1: _NormalizableURIInput, ph: PropertyHelper) =>
    match \exhaustive\ NormalizeURI(arg1.uri)
    | let u: URI val =>
      match u.authority
      | let a: URIAuthority =>
        ph.assert_true(
          not _HasUpperASCIIOutsidePercent(a.host),
          "host has uppercase outside percent: " + a.host)
      end
    | let e: InvalidPercentEncoding val =>
      ph.fail("normalization failed: " + arg1.uri.string())
    end

class \nodoc\ iso _PropertyNormalizeNoEncodedUnreserved
  is Property1[_NormalizableURIInput]
  """
  After normalization, no %XX sequence decodes to an unreserved character.
  """
  fun name(): String => "uri/normalize_uri/no_encoded_unreserved"

  fun gen(): Generator[_NormalizableURIInput] =>
    _NormalizableURIInputGenerator()

  fun ref property(arg1: _NormalizableURIInput, ph: PropertyHelper) =>
    match \exhaustive\ NormalizeURI(arg1.uri)
    | let u: URI val =>
      _check_no_encoded_unreserved(ph, u.path, "path")
      match u.authority
      | let a: URIAuthority =>
        _check_no_encoded_unreserved(ph, a.host, "host")
        match a.userinfo
        | let ui: String =>
          _check_no_encoded_unreserved(ph, ui, "userinfo")
        end
      end
      match u.query
      | let q: String =>
        _check_no_encoded_unreserved(ph, q, "query")
      end
      match u.fragment
      | let f: String =>
        _check_no_encoded_unreserved(ph, f, "fragment")
      end
    | let e: InvalidPercentEncoding val =>
      ph.fail("normalization failed: " + arg1.uri.string())
    end

  fun _check_no_encoded_unreserved(
    ph: PropertyHelper,
    s: String val,
    label: String)
  =>
    var i: USize = 0
    while i < s.size() do
      try
        if s(i)? == '%' then
          if (i + 2) < s.size() then
            let hi = PercentDecode._hex_value(s(i + 1)?)?
            let lo = PercentDecode._hex_value(s(i + 2)?)?
            let byte = (hi * 16) + lo
            ph.assert_false(
              PercentEncode._is_unreserved(byte),
              label + " has encoded unreserved char: " +
                s.substring(i.isize(), (i + 3).isize()))
          end
          i = i + 3
        else
          i = i + 1
        end
      else
        return
      end
    end

class \nodoc\ iso _PropertyNormalizeUppercaseHex
  is Property1[_NormalizableURIInput]
  """
  After normalization, all %XX sequences use uppercase hex digits.
  """
  fun name(): String => "uri/normalize_uri/uppercase_hex"

  fun gen(): Generator[_NormalizableURIInput] =>
    _NormalizableURIInputGenerator()

  fun ref property(arg1: _NormalizableURIInput, ph: PropertyHelper) =>
    match \exhaustive\ NormalizeURI(arg1.uri)
    | let u: URI val =>
      _check_uppercase_hex(ph, u.path, "path")
      match u.authority
      | let a: URIAuthority =>
        _check_uppercase_hex(ph, a.host, "host")
        match a.userinfo
        | let ui: String =>
          _check_uppercase_hex(ph, ui, "userinfo")
        end
      end
      match u.query
      | let q: String =>
        _check_uppercase_hex(ph, q, "query")
      end
      match u.fragment
      | let f: String =>
        _check_uppercase_hex(ph, f, "fragment")
      end
    | let e: InvalidPercentEncoding val =>
      ph.fail("normalization failed: " + arg1.uri.string())
    end

  fun _check_uppercase_hex(
    ph: PropertyHelper,
    s: String val,
    label: String)
  =>
    var i: USize = 0
    while i < s.size() do
      try
        if s(i)? == '%' then
          if (i + 2) < s.size() then
            let h1 = s(i + 1)?
            let h2 = s(i + 2)?
            ph.assert_false(
              ((h1 >= 'a') and (h1 <= 'f')) or
                ((h2 >= 'a') and (h2 <= 'f')),
              label + " has lowercase hex: " +
                s.substring(i.isize(), (i + 3).isize()))
          end
          i = i + 3
        else
          i = i + 1
        end
      else
        return
      end
    end

class \nodoc\ iso _PropertyNormalizeNoDotSegments
  is Property1[_NormalizableURIInput]
  """
  After normalization, the path has no dot segments.
  """
  fun name(): String => "uri/normalize_uri/no_dot_segments"

  fun gen(): Generator[_NormalizableURIInput] =>
    _NormalizableURIInputGenerator()

  fun ref property(arg1: _NormalizableURIInput, ph: PropertyHelper) =>
    match \exhaustive\ NormalizeURI(arg1.uri)
    | let u: URI val =>
      let path = u.path
      ph.assert_eq[String val](
        RemoveDotSegments(path),
        path,
        "path still has dot segments: " + path +
          " from: " + arg1.uri.string())
    | let e: InvalidPercentEncoding val =>
      ph.fail("normalization failed: " + arg1.uri.string())
    end

class \nodoc\ iso _PropertyNormalizeParseRoundtrip
  is Property1[_NormalizableURIInput]
  """
  Parsing the string form of a normalized URI produces an equal URI.
  """
  fun name(): String => "uri/normalize_uri/parse_roundtrip"

  fun gen(): Generator[_NormalizableURIInput] =>
    _NormalizableURIInputGenerator()

  fun ref property(arg1: _NormalizableURIInput, ph: PropertyHelper) =>
    match \exhaustive\ NormalizeURI(arg1.uri)
    | let normalized: URI val =>
      match \exhaustive\ ParseURI(normalized.string())
      | let reparsed: URI val =>
        ph.assert_true(
          normalized == reparsed,
          "roundtrip failed: normalized=" + normalized.string() +
            " reparsed=" + reparsed.string())
      | let e: URIParseError val =>
        ph.fail("reparse failed for: " + normalized.string() +
          " error: " + e.string())
      end
    | let e: InvalidPercentEncoding val =>
      ph.fail("normalization failed: " + arg1.uri.string())
    end

class \nodoc\ iso _PropertyNormalizeNoDefaultPort
  is Property1[_NormalizableURIInput]
  """
  After normalization, if the scheme has a known default port, the port
  is not that default.
  """
  fun name(): String => "uri/normalize_uri/no_default_port"

  fun gen(): Generator[_NormalizableURIInput] =>
    _NormalizableURIInputGenerator()

  fun ref property(arg1: _NormalizableURIInput, ph: PropertyHelper) =>
    match \exhaustive\ NormalizeURI(arg1.uri)
    | let u: URI val =>
      match u.scheme
      | let scheme: String =>
        match _SchemeDefaultPort(scheme)
        | let default_port: U16 =>
          match u.authority
          | let a: URIAuthority =>
            match a.port
            | let p: U16 =>
              ph.assert_false(
                p == default_port,
                "default port " + default_port.string() +
                  " not removed for scheme " + scheme)
            end
          end
        end
      end
    | let e: InvalidPercentEncoding val =>
      ph.fail("normalization failed: " + arg1.uri.string())
    end

class \nodoc\ iso _PropertyNormalizeNoEmptyPathWithAuthority
  is Property1[_NormalizableURIInput]
  """
  After normalization, if the scheme is http or https and authority is
  present, the path is not empty.
  """
  fun name(): String => "uri/normalize_uri/no_empty_path_with_authority"

  fun gen(): Generator[_NormalizableURIInput] =>
    _NormalizableURIInputGenerator()

  fun ref property(arg1: _NormalizableURIInput, ph: PropertyHelper) =>
    match \exhaustive\ NormalizeURI(arg1.uri)
    | let u: URI val =>
      match u.scheme
      | let scheme: String =>
        if (scheme == "http") or (scheme == "https") then
          match u.authority
          | let _: URIAuthority =>
            ph.assert_false(
              u.path == "",
              "empty path with authority for " + scheme + ": " +
                u.string())
          end
        end
      end
    | let e: InvalidPercentEncoding val =>
      ph.fail("normalization failed: " + arg1.uri.string())
    end

class \nodoc\ iso _PropertyNormalizeEquivalentConsistent
  is Property1[(_NormalizableURIInput, _NormalizableURIInput)]
  """
  URIEquivalent(a, b) is consistent with NormalizeURI(a) == NormalizeURI(b).
  """
  fun name(): String => "uri/normalize_uri/equivalent_consistent"

  fun gen(): Generator[(_NormalizableURIInput, _NormalizableURIInput)] =>
    Generators.zip2[_NormalizableURIInput, _NormalizableURIInput](
      _NormalizableURIInputGenerator(),
      _NormalizableURIInputGenerator())

  fun ref property(
    arg1: (_NormalizableURIInput, _NormalizableURIInput),
    ph: PropertyHelper)
  =>
    (let a_in, let b_in) = arg1
    let a = a_in.uri
    let b = b_in.uri
    match \exhaustive\ (NormalizeURI(a), NormalizeURI(b))
    | (let norm_a: URI val, let norm_b: URI val) =>
      let expected = norm_a == norm_b
      match \exhaustive\ URIEquivalent(a, b)
      | let actual: Bool =>
        ph.assert_eq[Bool](
          expected,
          actual,
          "equivalent inconsistent: a=" + a.string() +
            " b=" + b.string() +
            " norm_a=" + norm_a.string() +
            " norm_b=" + norm_b.string())
      | let e: InvalidPercentEncoding val =>
        ph.fail("URIEquivalent failed: " + a.string() +
          " vs " + b.string())
      end
    | (let _: InvalidPercentEncoding val, _) =>
      ph.fail("normalization failed for a: " + a.string())
    | (_, let _: InvalidPercentEncoding val) =>
      ph.fail("normalization failed for b: " + b.string())
    end

class \nodoc\ iso _PropertyNormalizeInvalidPercentRejected
  is Property1[String val]
  """
  URIs with malformed percent-encoding produce InvalidPercentEncoding.
  """
  fun name(): String => "uri/normalize_uri/invalid_percent_rejected"

  fun gen(): Generator[String val] =>
    Generators.one_of[String val](
      [
        "http://example.com/path%GG"
      "http://example.com/path%0"
      "http://example.com/path%"
      "http://example.com/%ZZfoo"
      "http://example.com/path?q=%X1"
      "http://example.com/path#frag%"
      "http://user%info@example.com/path"
    ])

  fun ref property(arg1: String val, ph: PropertyHelper) =>
    match \exhaustive\ ParseURI(arg1)
    | let u: URI val =>
      match \exhaustive\ NormalizeURI(u)
      | let _: URI val =>
        ph.fail("expected InvalidPercentEncoding for: " + arg1)
      | let _: InvalidPercentEncoding val =>
        ph.assert_true(true)
      end
    | let _: URIParseError val =>
      // Parse error is also acceptable — input is malformed
      ph.assert_true(true)
    end

// -- Known-good example tests --
class \nodoc\ iso _TestNormalizeURIKnownGood is UnitTest
  """
  Known-good normalization examples from RFC 3986 section 6.2.2 and 6.2.3.
  """
  fun name(): String => "uri/normalize_uri/known_good"

  fun ref apply(h: TestHelper) =>
    // -- 6.2.2 Syntax-based normalization --

    // Case normalization (scheme + host)
    _assert_normalize(
      h, "HTTP://Example.COM/", "http://example.com/")

    // Percent-encoding normalization (unreserved decoded)
    _assert_normalize(
      h, "http://example.com/%7Euser", "http://example.com/~user")

    // Dot-segment removal
    _assert_normalize(
      h, "http://example.com/a/../b", "http://example.com/b")

    // Percent-encoded dot segments decoded then removed
    _assert_normalize(
      h, "http://example.com/%2E%2E/b", "http://example.com/b")

    // Query percent-encoding normalization
    _assert_normalize(
      h, "http://example.com/path?q=%6A", "http://example.com/path?q=j")

    // Already-normalized URI unchanged
    _assert_normalize(
      h, "http://example.com/path", "http://example.com/path")

    // Relative reference (no scheme) — scheme normalization skipped
    _assert_normalize(
      h, "/A/../b/%7Epath", "/b/~path")

    // IP-literal host lowercased
    _assert_normalize(
      h, "http://[FE80::1]/", "http://[fe80::1]/")

    // -- 6.2.3 Scheme-based normalization --

    // Default port removal: http
    _assert_normalize(
      h, "http://example.com:80/", "http://example.com/")

    // Default port removal: https
    _assert_normalize(
      h, "https://example.com:443/", "https://example.com/")

    // Default port removal: ftp
    _assert_normalize(
      h, "ftp://example.com:21/", "ftp://example.com/")

    // Non-default port kept
    _assert_normalize(
      h, "http://example.com:8080/", "http://example.com:8080/")

    // Unknown scheme — port kept even if it matches a known default
    _assert_normalize(
      h, "unknown://example.com:80/", "unknown://example.com:80/")

    // Empty path with authority → "/" (http)
    _assert_normalize(
      h, "http://example.com", "http://example.com/")

    // Empty path with query → "/" inserted (http)
    _assert_normalize(
      h, "http://example.com?query", "http://example.com/?query")

    // ftp: empty path NOT normalized to "/"
    _assert_normalize(
      h, "ftp://example.com", "ftp://example.com")

    // -- Combined 6.2.2 + 6.2.3 --
    _assert_normalize(
      h,
      "HTTP://Example.COM:80/%7Euser/a/../b?q=%6A",
      "http://example.com/~user/b?q=j")

  fun _assert_normalize(
    h: TestHelper,
    input: String,
    expected: String)
  =>
    match \exhaustive\ ParseURI(input)
    | let u: URI val =>
      match \exhaustive\ NormalizeURI(u)
      | let normalized: URI val =>
        h.assert_eq[String val](
          expected,
          normalized.string(),
          "NormalizeURI(" + input + ")")
      | let e: InvalidPercentEncoding val =>
        h.fail("normalization failed for " + input + ": " + e.string())
      end
    | let e: URIParseError val =>
      h.fail("parse failed for " + input + ": " + e.string())
    end

class \nodoc\ iso _TestURIEquivalentKnownGood is UnitTest
  """
  Known-good equivalence comparisons.
  """
  fun name(): String => "uri/uri_equivalent/known_good"

  fun ref apply(h: TestHelper) =>
    // Equivalent: case + default port
    _assert_equivalent(
      h, "HTTP://Example.COM:80/path", "http://example.com/path", true)

    // Not equivalent: different paths
    _assert_equivalent(
      h, "http://example.com/a", "http://example.com/b", false)

    // Reflexive: any URI equivalent to itself
    _assert_equivalent(
      h,
      "http://example.com/path?q=1#frag",
      "http://example.com/path?q=1#frag",
      true)

  fun _assert_equivalent(
    h: TestHelper,
    a_str: String,
    b_str: String,
    expected: Bool)
  =>
    match \exhaustive\ (ParseURI(a_str), ParseURI(b_str))
    | (let a: URI val, let b: URI val) =>
      match \exhaustive\ URIEquivalent(a, b)
      | let result: Bool =>
        h.assert_eq[Bool](
          expected,
          result,
          "URIEquivalent(" + a_str + ", " + b_str + ")")
      | let e: InvalidPercentEncoding val =>
        h.fail("equivalence failed for (" + a_str + ", " + b_str +
          "): " + e.string())
      end
    else
      h.fail("parse failed for (" + a_str + ", " + b_str + ")")
    end

// -- Helpers --
primitive \nodoc\ _HasUpperASCII
  """
  Check if a string contains any uppercase ASCII letter.
  """
  fun apply(s: String val): Bool =>
    for c in s.values() do
      if (c >= 'A') and (c <= 'Z') then
        return true
      end
    end
    false

primitive \nodoc\ _HasUpperASCIIOutsidePercent
  """
  Check if a string contains uppercase ASCII letters outside of %XX
  sequences. Hex digits within percent triplets are allowed to be
  uppercase (that's the normalized form).
  """
  fun apply(s: String val): Bool =>
    var i: USize = 0
    while i < s.size() do
      try
        let c = s(i)?
        if c == '%' then
          // Skip the percent triplet
          i = i + 3
        elseif (c >= 'A') and (c <= 'Z') then
          return true
        else
          i = i + 1
        end
      else
        return false
      end
    end
    false

// -- Generators --
class \nodoc\ val _NormalizableURIInput
  let uri: URI val

  new val create(uri': URI val) =>
    uri = uri'

primitive \nodoc\ _NormalizableURIInputGenerator
  """
  Generate URIs suitable for normalization testing. All percent-encoding
  is syntactically valid. Components may have mixed case, percent-encoded
  unreserved characters, and dot segments — all things normalization
  should fix.
  """
  fun apply(): Generator[_NormalizableURIInput] =>
    Generators.map4[
      (String val | None),
      _NormAuthority,
      _NormPathQueryFrag,
      _NormPathQueryFrag,
      _NormalizableURIInput](
      _scheme_gen(),
      _authority_gen(),
      _path_query_frag_gen(),
      _path_query_frag_gen(),
      {(scheme, auth, pqf1, pqf2) =>
        // Build a URI string from components and parse it
        let out = recover iso String(128) end
        match scheme
        | let s: String => out.append(s); out.push(':')
        end
        match auth.value
        | let a: String => out.append("//"); out.append(a)
        end
        out.append(pqf1.path)
        match pqf1.query
        | let q: String => out.push('?'); out.append(q)
        end
        match pqf2.query
        | let f: String => out.push('#'); out.append(f)
        end
        let uri_str: String val = consume out
        match ParseURI(uri_str)
        | let u: URI val => _NormalizableURIInput(u)
        else
          // Fallback: should not happen since inputs are valid
          _NormalizableURIInput(
            URI(None, None, "/fallback", None, None))
        end
      })

  fun _scheme_gen(): Generator[(String val | None)] =>
    Generators.frequency[(String val | None)](
      [
        as WeightedGenerator[(String val | None)]:
        (1, Generators.unit[(String val | None)](None))
        (3, Generators.one_of[String val](
          ["HTTP"; "Http"; "https"; "FTP"; "Scheme"])
          .map[(String val | None)]({(s) => s }))
      ])

  fun _authority_gen(): Generator[_NormAuthority] =>
    Generators.map2[
      (String val | None),
      _NormHostPort,
      _NormAuthority](
      _userinfo_gen(),
      _host_port_gen(),
      {(userinfo, host_port) =>
        match host_port.value
        | let hp: String =>
          let auth =
            match userinfo
            | let u: String => u + "@" + hp
            else hp
            end
          _NormAuthority(auth)
        else
          _NormAuthority(None)
        end
      })

  fun _userinfo_gen(): Generator[(String val | None)] =>
    Generators.frequency[(String val | None)](
      [
        as WeightedGenerator[(String val | None)]:
        (3, Generators.unit[(String val | None)](None))
        (1, Generators.one_of[String val](
          ["user"; "us%65r"; "user:pass"])
          .map[(String val | None)]({(s) => s }))
      ])

  fun _host_port_gen(): Generator[_NormHostPort] =>
    Generators.map2[String val, (String val | None), _NormHostPort](
      _host_gen(),
      _port_gen(),
      {(host, port) =>
        match port
        | let p: String => _NormHostPort(host + ":" + p)
        else _NormHostPort(host)
        end
      })

  fun _host_gen(): Generator[String val] =>
    Generators.one_of[String val](
      [
        "EXAMPLE.COM"
        "Example.Org"
        "LOCALHOST"
        "ex%61mple.com"
        "[FE80::1]"
        "192.168.1.1"
      ])

  fun _port_gen(): Generator[(String val | None)] =>
    Generators.frequency[(String val | None)](
      [
        as WeightedGenerator[(String val | None)]:
        (2, Generators.unit[(String val | None)](None))
        (1, Generators.one_of[String val](
          ["80"; "443"; "21"; "8080"; "3000"])
          .map[(String val | None)]({(s) => s }))
      ])

  fun _path_query_frag_gen(): Generator[_NormPathQueryFrag] =>
    Generators.map2[String val, (String val | None), _NormPathQueryFrag](
      _path_gen(),
      _query_or_frag_gen(),
      {(path, query) => _NormPathQueryFrag(path, query) })

  fun _path_gen(): Generator[String val] =>
    Generators.one_of[String val](
      [
        "/a/../b"
        "/%7Epath"
        "/a/%2E%2E/b"
        "/normal/path"
        ""
        "/"
        "/a/b/c"
      ])

  fun _query_or_frag_gen(): Generator[(String val | None)] =>
    Generators.frequency[(String val | None)](
      [
        as WeightedGenerator[(String val | None)]:
        (2, Generators.unit[(String val | None)](None))
        (1, Generators.one_of[String val](
          ["q=%6a"; "key=%7Eval"; "plain"])
          .map[(String val | None)]({(s) => s }))
      ])

// Helper types for generator composition
class \nodoc\ val _NormAuthority
  let value: (String | None)

  new val create(value': (String | None)) => value = value'

class \nodoc\ val _NormHostPort
  let value: (String | None)

  new val create(value': (String | None)) => value = value'

class \nodoc\ val _NormPathQueryFrag
  let path: String
  let query: (String | None)

  new val create(path': String, query': (String | None)) =>
    path = path'
    query = query'
