use "uri"

actor Main
  """
  Demonstrates form-urlencoded parsing.
  """
  new create(env: Env) =>
    env.out.print("Form URL-Encoded Parsing Examples")
    env.out.print("=================================")
    env.out.print("")

    // Parse a form-urlencoded string (e.g., from an HTTP POST body)
    env.out.print("Parsing a standalone string:")
    match \exhaustive\ ParseFormURLEncoded(
          "username=jane&color=blue&color=green")
    | let params: FormURLEncoded val =>
      // Look up a single value
      match params.get("username")
      | let v: String => env.out.print("  username: " + v)
      end

      // Duplicate keys — get_all() returns every value in order
      let colors = params.get_all("color")
      env.out.print("  colors:   " + ", ".join(colors.values()))

      // Presence check
      env.out.print("  has username? " + params.contains("username").string())
      env.out.print("  has email?    " + params.contains("email").string())

      // Iterate all pairs
      env.out.print("  all pairs (" + params.size().string() + "):")
      for (k, v) in params.pairs() do
        env.out.print("    " + k + " = " + v)
      end
    | let e: InvalidPercentEncoding val =>
      env.out.print("  Decode error: " + e.string())
    end

    env.out.print("")

    // Plus signs decode as spaces, percent-encoding is decoded
    env.out.print("Decoding special characters:")
    match \exhaustive\ ParseFormURLEncoded(
          "greeting=hello+world&path=%2Fhome%2Fuser")
    | let params: FormURLEncoded val =>
      match params.get("greeting")
      | let v: String => env.out.print("  greeting: " + v)
      end
      match params.get("path")
      | let v: String => env.out.print("  path:     " + v)
      end
    | let e: InvalidPercentEncoding val =>
      env.out.print("  Decode error: " + e.string())
    end

    env.out.print("")

    // Error handling — invalid percent-encoding
    env.out.print("Error handling:")
    match \exhaustive\ ParseFormURLEncoded("key=%ZZ")
    | let _: FormURLEncoded val =>
      env.out.print("  (unexpected success)")
    | let e: InvalidPercentEncoding val =>
      env.out.print("  Expected error: " + e.string())
    end

    env.out.print("")

    // URI.query_params() — convenience wrapper for query strings
    env.out.print("Parsing query parameters from a URI:")
    match \exhaustive\ ParseURI(
          "https://example.com/search?q=pony+lang&limit=10")
    | let u: URI val =>
      match u.query_params()
      | let params: FormURLEncoded val =>
        match params.get("q")
        | let v: String => env.out.print("  q:     " + v)
        end
        match params.get("limit")
        | let v: String => env.out.print("  limit: " + v)
        end
      end
    | let e: URIParseError val =>
      env.out.print("  Parse error: " + e.string())
    end
