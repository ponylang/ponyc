"""
# URI Template Expansion (RFC 6570)

Expand URI templates by substituting variables according to operator-specific
rules for encoding, separators, and naming. All four levels of RFC 6570 are
supported.

For one-shot expansion, use `URITemplateBuilder` — it combines parsing, variable
binding, and expansion into a single fluent chain:

```pony
use "uri/template"

actor Main
  new create(env: Env) =>
    try
      let uri = URITemplateBuilder("{scheme}://{host}{/path*}")
        .set("scheme", "https")
        .set("host", "example.com")
        .set_list("path", ["api"; "v1"; "users"])
        .build()?
      env.out.print(uri)
      // => https://example.com/api/v1/users
    end
```

For repeated expansion of the same template or detailed error reporting, use
`URITemplate` and `URITemplateParse`:

```pony
    let vars = URITemplateVariables
    vars.set("host", "example.com")
    vars.set_list("path", ["api"; "v1"; "users"])
    vars.set_pairs("query", [("page", "1"); ("limit", "10")])

    match URITemplateParse("https://{host}{/path*}{?query*}")
    | let tpl: URITemplate =>
      env.out.print(tpl.expand(vars))
      // => https://example.com/api/v1/users?page=1&limit=10
    | let err: URITemplateParseError =>
      env.err.print("Bad template: " + err.string())
    end
```
"""
