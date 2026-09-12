use "uri"

actor Main
  """
  Demonstrates URI construction and modification with URIBuilder.
  """
  new create(env: Env) =>
    env.out.print("URI Building Examples")
    env.out.print("=====================")
    env.out.print("")

    // Build a URI from scratch
    env.out.print("Building from scratch:")
    match \exhaustive\ URIBuilder
      .set_scheme("https")
      .set_host("example.com")
      .append_path_segment("api")
      .append_path_segment("v1")
      .append_path_segment("users")
      .add_query_param("name", "Jane Doe")
      .add_query_param("page", "1")
      .set_fragment("results")
      .build()
    | let u: URI val =>
      env.out.print("  " + u.string())
    | let e: URIBuildError val =>
      env.out.print("  Build error: " + e.string())
    end

    env.out.print("")

    // Modify an existing URI
    env.out.print("Modifying an existing URI:")
    match \exhaustive\ ParseURI("https://example.com/old/path?x=1")
    | let original: URI val =>
      env.out.print("  Original:  " + original.string())
      match \exhaustive\ URIBuilder.from(original)
        .set_path("/new/path")
        .add_query_param("y", "2")
        .set_fragment("top")
        .build()
      | let modified: URI val =>
        env.out.print("  Modified:  " + modified.string())
      | let e: URIBuildError val =>
        env.out.print("  Build error: " + e.string())
      end
    | let e: URIParseError val =>
      env.out.print("  Parse error: " + e.string())
    end

    env.out.print("")

    // Error handling for invalid scheme
    env.out.print("Error handling:")
    match \exhaustive\ URIBuilder
      .set_scheme("1nvalid").set_host("example.com").build()
    | let _: URI val =>
      env.out.print("  (unexpected success)")
    | let e: URIBuildError val =>
      env.out.print("  Expected error: " + e.string())
    end
