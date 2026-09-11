class URITemplateBuilder
  """
  A convenience builder for one-shot URI template expansion.

  Combines template parsing, variable binding, and expansion into a single
  fluent chain. Use this when you need to expand a template once and don't
  need to reuse the parsed template or inspect parse errors.

  For repeated expansion of the same template, parse once with `URITemplate`
  and call `expand()` multiple times. For detailed parse error reporting, use
  `URITemplateParse`.

  ```pony
  let uri = URITemplateBuilder("{scheme}://{host}{/path*}")
    .set("scheme", "https")
    .set("host", "example.com")
    .set_list("path", ["api"; "v1"; "users"])
    .build()?
  // => "https://example.com/api/v1/users"
  ```
  """
  let _template: String
  let _vars: URITemplateVariables

  new create(template: String) =>
    """
    Create a builder for the given URI template string.

    The template is not parsed until `build()` is called, so construction
    always succeeds.
    """
    _template = template
    _vars = URITemplateVariables

  fun ref set(name: String, value: String): URITemplateBuilder ref =>
    """
    Bind a string variable. Returns this builder for chaining.
    """
    _vars.set(name, value)
    this

  fun ref set_list(
    name: String,
    values: Array[String val] val)
    : URITemplateBuilder ref
  =>
    """
    Bind a list variable. Returns this builder for chaining.
    """
    _vars.set_list(name, values)
    this

  fun ref set_pairs(
    name: String,
    pairs: Array[(String val, String val)] val)
    : URITemplateBuilder ref
  =>
    """
    Bind an associative array variable. Returns this builder for chaining.
    """
    _vars.set_pairs(name, pairs)
    this

  fun box build(): String iso^ ? =>
    """
    Parse the template and expand it with the bound variables.

    Raises an error if the template has invalid syntax. Use `URITemplateParse`
    instead when you need a description of what went wrong.
    """
    URITemplate(_template)?.expand(_vars)
