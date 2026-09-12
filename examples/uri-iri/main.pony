use "uri"

actor Main
  """
  Demonstrates IRI support (RFC 3987).
  """
  new create(env: Env) =>
    env.out.print("IRI Examples (RFC 3987)")
    env.out.print("=======================")
    env.out.print("")

    // ParseURI handles IRIs natively — non-ASCII bytes pass through
    let iri_str = "http://example.com/r\xE9sum\xE9?q=caf\xE9#\xE9"
    env.out.print("Parsing an IRI:")
    match \exhaustive\ ParseURI(iri_str)
    | let u: URI val =>
      env.out.print("  Input:     " + iri_str)
      env.out.print("  Parsed:    " + u.string())
      env.out.print("  Path:      " + u.path)
      match u.query
      | let q: String => env.out.print("  Query:     " + q)
      end
      match u.fragment
      | let f: String => env.out.print("  Fragment:  " + f)
      end
    | let e: URIParseError val =>
      env.out.print("  Parse error: " + e.string())
    end

    env.out.print("")

    // Convert between IRI and URI forms
    env.out.print("IRI to URI Conversion")
    env.out.print("---------------------")
    match \exhaustive\ ParseURI("http://example.com/caf\xE9")
    | let iri: URI val =>
      let uri = IRIToURI(iri)
      env.out.print("  IRI: " + iri.string())
      env.out.print("  URI: " + uri.string())
    | let e: URIParseError val =>
      env.out.print("  Error: " + e.string())
    end

    env.out.print("")

    // Convert URI back to IRI
    env.out.print("URI to IRI Conversion")
    env.out.print("---------------------")
    match \exhaustive\ ParseURI("http://example.com/r%C3%A9sum%C3%A9")
    | let uri: URI val =>
      let iri = URIToIRI(uri)
      env.out.print("  URI: " + uri.string())
      env.out.print("  IRI: " + iri.string())
    | let e: URIParseError val =>
      env.out.print("  Error: " + e.string())
    end

    env.out.print("")

    // IRI-aware percent-encoding
    env.out.print("IRI Percent-Encoding")
    env.out.print("--------------------")
    let raw = "caf\xE9 menu"
    env.out.print("  Input:   " + raw)
    env.out.print("  IRI:     " + IRIPercentEncode(raw, URIPartPath))
    env.out.print("  URI:     " + PercentEncode(raw, URIPartPath))

    env.out.print("")

    // IRI normalization
    env.out.print("IRI Normalization")
    env.out.print("-----------------")
    let unnormalized = "HTTP://Example.COM:80/r%C3%A9sum%C3%A9"
    match \exhaustive\ ParseURI(unnormalized)
    | let u: URI val =>
      match \exhaustive\ NormalizeIRI(u)
      | let n: URI val =>
        env.out.print("  Original:   " + unnormalized)
        env.out.print("  Normalized: " + n.string())
      | let e: InvalidPercentEncoding val =>
        env.out.print("  Error: " + e.string())
      end
    | let e: URIParseError val =>
      env.out.print("  Error: " + e.string())
    end

    env.out.print("")

    // IRI equivalence — detects equivalence across IRI/URI forms
    env.out.print("IRI Equivalence")
    env.out.print("---------------")
    let eq_a = "http://example.com/caf\xE9"
    let eq_b = "http://example.com/caf%C3%A9"
    match (ParseURI(eq_a), ParseURI(eq_b))
    | (let a: URI val, let b: URI val) =>
      match \exhaustive\ IRIEquivalent(a, b)
      | let result: Bool =>
        env.out.print("  " + eq_a + " == " + eq_b + "? " +
          result.string())
      | let e: InvalidPercentEncoding val =>
        env.out.print("  Error: " + e.string())
      end
    else
      env.out.print("  Parse error")
    end
