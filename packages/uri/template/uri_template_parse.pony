primitive URITemplateParse
  """
  Parse a URI template string, returning either a valid `URITemplate`
  or a `URITemplateParseError` describing what went wrong.

  Use this instead of the `URITemplate` constructor when you need details
  about parse failures.

  ```pony
  match URITemplateParse("{+path}/here")
  | let tpl: URITemplate =>
    let result = tpl.expand(vars)
  | let err: URITemplateParseError =>
    env.err.print("Bad template: " + err.string())
  end
  ```
  """
  fun apply(template: String): (URITemplate | URITemplateParseError) =>
    """
    Parse a URI template string.

    Returns a `URITemplate` on success or a `URITemplateParseError` with
    details about the syntax error on failure.
    """
    match \exhaustive\ _URITemplateParser.parse(template)
    | let parts: Array[_TemplatePart] val =>
      URITemplate._from_parts(template, parts)
    | let err: URITemplateParseError =>
      err
    end
