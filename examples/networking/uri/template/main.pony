use "uri/template"

actor Main
  """
  Demonstrates URI template expansion (RFC 6570).
  """
  new create(env: Env) =>
    env.out.print("URI Template Expansion Examples")
    env.out.print("===============================")
    env.out.print("")

    // Build variable bindings
    let vars = URITemplateVariables
    vars.set("scheme", "https")
    vars.set("host", "example.com")
    vars.set("path", "/api/v1")
    vars.set("user", "fred")
    vars.set("query", "pony lang")
    vars.set_list("segments", recover val ["api"; "v1"; "users"] end)
    vars.set_pairs(
      "params",
      recover val [("page", "1"); ("limit", "10")] end)

    // Simple expansion
    _expand(env, "{scheme}://{host}{path}", vars)
    // => https://example.com/api/v1

    // Reserved expansion (preserves reserved chars like /)
    _expand(env, "{+path}/users/{user}", vars)
    // => /api/v1/users/fred

    // Fragment expansion
    _expand(env, "https://{host}/docs{#user}", vars)
    // => https://example.com/docs#fred

    // Path segment expansion with exploded list
    _expand(env, "https://{host}{/segments*}", vars)
    // => https://example.com/api/v1/users

    // Query expansion with exploded pairs
    _expand(env, "https://{host}/search{?params*}", vars)
    // => https://example.com/search?page=1&limit=10

    // Query with encoded value
    _expand(env, "https://{host}/search{?query}", vars)
    // => https://example.com/search?query=pony%20lang

    // Prefix modifier
    _expand(env, "{scheme}://{host}/{user:2}", vars)
    // => https://example.com/fr

    env.out.print("")

    // Error handling with URITemplateParse
    env.out.print("Error handling:")
    match \exhaustive\ URITemplateParse("{=invalid}")
    | let tpl: URITemplate =>
      env.out.print("  (unexpected success)")
    | let err: URITemplateParseError =>
      env.out.print("  " + err.string())
    end

  fun _expand(
    env: Env,
    template: String,
    vars: URITemplateVariables box)
  =>
    try
      let tpl = URITemplate(template)?
      let result: String val = tpl.expand(vars)
      env.out.print("  " + template)
      env.out.print("  => " + result)
      env.out.print("")
    else
      env.out.print("  Failed to parse: " + template)
    end
