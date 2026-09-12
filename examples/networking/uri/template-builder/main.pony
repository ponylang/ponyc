use "uri/template"

actor Main
  """
  Demonstrates URITemplateBuilder for one-shot template expansion.
  """
  new create(env: Env) =>
    env.out.print("URI Template Builder Examples")
    env.out.print("=============================")
    env.out.print("")

    // One-shot expansion with string, list, and pairs variables
    env.out.print("Full expansion with all variable types:")
    try
      let uri = URITemplateBuilder("{scheme}://{host}{/path*}{?query*}")
        .set("scheme", "https")
        .set("host", "example.com")
        .set_list(
          "path",
          recover val ["api"; "v1"; "users"] end)
        .set_pairs(
          "query",
          recover val [("page", "1"); ("limit", "10")] end)
        .build()?
      env.out.print("  " + consume uri)
      // => https://example.com/api/v1/users?page=1&limit=10
    else
      env.out.print("  (failed)")
    end

    env.out.print("")

    // Minimal usage with just string variables
    env.out.print("Simple string expansion:")
    try
      let uri = URITemplateBuilder("{scheme}://{host}/users/{user}")
        .set("scheme", "https")
        .set("host", "example.com")
        .set("user", "fred")
        .build()?
      env.out.print("  " + consume uri)
      // => https://example.com/users/fred
    else
      env.out.print("  (failed)")
    end

    env.out.print("")

    // Error handling for invalid templates
    env.out.print("Invalid template:")
    try
      let uri = URITemplateBuilder("{=invalid}").build()?
      env.out.print("  (unexpected success: " + consume uri + ")")
    else
      env.out.print("  build() correctly raised an error")
    end
