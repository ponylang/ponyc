use "uri"

actor Main
  """
  Demonstrates URI parsing (RFC 3986).
  """
  new create(env: Env) =>
    env.out.print("URI Parsing Examples")
    env.out.print("====================")
    env.out.print("")

    // Parse a full URI
    match \exhaustive\ ParseURI(
      "http://user@example.com:8080/path?query=1#frag")
    | let u: URI val =>
      env.out.print("Parsed: " + u.string())
      match u.scheme
      | let s: String => env.out.print("  scheme:    " + s)
      end
      match u.authority
      | let a: URIAuthority =>
        env.out.print("  authority: " + a.string())
        env.out.print("  host:      " + a.host)
        match a.port
        | let p: U16 => env.out.print("  port:      " + p.string())
        end
      end
      env.out.print("  path:      " + u.path)
      match u.query
      | let q: String => env.out.print("  query:     " + q)
      end
      match u.fragment
      | let f: String => env.out.print("  fragment:  " + f)
      end
    | let e: URIParseError val =>
      env.out.print("Parse error: " + e.string())
    end

    env.out.print("")

    // Query parameter lookup
    match \exhaustive\ ParseURI(
          "https://example.com/search?q=pony&page=1&page=2")
    | let u: URI val =>
      env.out.print("Query parameters for: " + u.string())
      match u.query_params()
      | let params: FormURLEncoded val =>
        match params.get("q")
        | let v: String => env.out.print("  q:    " + v)
        end
        let pages = params.get_all("page")
        env.out.print("  page: " + ", ".join(pages.values()))
      end
    | let e: URIParseError val =>
      env.out.print("Parse error: " + e.string())
    end

    env.out.print("")

    // Standalone authority parsing (HTTP CONNECT authority-form)
    env.out.print("Standalone authority parsing:")
    match \exhaustive\ ParseURIAuthority("example.com:443")
    | let a: URIAuthority val =>
      env.out.print("  host: " + a.host)
      match a.port
      | let p: U16 => env.out.print("  port: " + p.string())
      end
    | let e: URIParseError val =>
      env.out.print("Parse error: " + e.string())
    end

    env.out.print("")

    // Path segments (split on / and percent-decode each segment)
    env.out.print("Path segments:")
    match \exhaustive\ PathSegments("/api/v1/hello%20world")
    | let segments: Array[String val] val =>
      for seg in segments.values() do
        env.out.print("  \"" + seg + "\"")
      end
    | let e: InvalidPercentEncoding val =>
      env.out.print("Decode error: " + e.string())
    end

    env.out.print("")

    // Percent encoding and decoding
    env.out.print("Percent encoding:")
    env.out.print("  encode(\"hello world/foo\", Path): " +
      PercentEncode("hello world/foo", URIPartPath))

    env.out.print("Percent decoding:")
    match \exhaustive\ PercentDecode("hello%20world")
    | let decoded: String val =>
      env.out.print("  decode(\"hello%20world\"): " + decoded)
    | let e: InvalidPercentEncoding val =>
      env.out.print("Decode error: " + e.string())
    end

    env.out.print("")

    // URI reference resolution (RFC 3986 section 5)
    env.out.print("URI Reference Resolution")
    env.out.print("========================")
    env.out.print("")

    match \exhaustive\ ParseURI("http://example.com/a/b/c")
    | let base: URI val =>
      env.out.print("Base: " + base.string())
      env.out.print("")

      // Relative path with dot segments
      _resolve(env, base, "../d")

      // Fragment-only reference
      _resolve(env, base, "#section2")

      // Absolute reference (ignores base entirely)
      _resolve(env, base, "https://other.com/page")
    | let e: URIParseError val =>
      env.out.print("Parse error: " + e.string())
    end

    env.out.print("")

    // URI normalization (RFC 3986 section 6)
    env.out.print("URI Normalization (RFC 3986 Section 6)")
    env.out.print("======================================")
    env.out.print("")

    let raw = "HTTP://Example.COM:80/%7Euser/a/../b?q=%6A"
    match \exhaustive\ ParseURI(raw)
    | let u: URI val =>
      match \exhaustive\ NormalizeURI(u)
      | let n: URI val =>
        env.out.print("  Original:   " + raw)
        env.out.print("  Normalized: " + n.string())
      | let e: InvalidPercentEncoding val =>
        env.out.print("  Normalization error: " + e.string())
      end
    | let e: URIParseError val =>
      env.out.print("  Parse error: " + e.string())
    end

    env.out.print("")

    // URI equivalence
    env.out.print("URI Equivalence")
    env.out.print("===============")
    env.out.print("")

    let eq_a = "http://EXAMPLE.COM:80/path"
    let eq_b = "http://example.com/path"
    match (ParseURI(eq_a), ParseURI(eq_b))
    | (let a: URI val, let b: URI val) =>
      match \exhaustive\ URIEquivalent(a, b)
      | let result: Bool =>
        env.out.print("  " + eq_a + " == " + eq_b + "? " +
          result.string())
      | let e: InvalidPercentEncoding val =>
        env.out.print("  Equivalence error: " + e.string())
      end
    else
      env.out.print("  Parse error")
    end

  fun _resolve(env: Env, base: URI val, reference_str: String) =>
    match \exhaustive\ ParseURI(reference_str)
    | let reference: URI val =>
      match \exhaustive\ ResolveURI(base, reference)
      | let result: URI val =>
        env.out.print("  resolve(\"" + reference_str + "\") = " +
          result.string())
      | let e: ResolveURIError val =>
        env.out.print("  resolve error: " + e.string())
      end
    | let e: URIParseError val =>
      env.out.print("  parse error: " + e.string())
    end
