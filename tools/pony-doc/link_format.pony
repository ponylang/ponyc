trait val LinkFormat
  """
  Format-specific behavior for cross-reference links and type parameter
  brackets in type rendering.

  TypeRenderer delegates link generation and bracket escaping to
  implementations of this trait. Pass `None` instead of a `LinkFormat`
  for plain-text output (no links, plain brackets).

  Implementations must ensure that `link()` returns a string appropriate
  for the output format. The returned string is embedded directly into the
  rendered output — it must not contain characters that are syntactically
  significant in the target format unless properly escaped.

  `open_bracket()` and `close_bracket()` must return a visually paired
  bracket pair suitable for delimiting type argument and type parameter
  lists in linked contexts.
  """
  fun link(name: String, tqfn: String): String
    """
    Produce a cross-reference string linking `name` to the entity
    identified by `tqfn`.
    """

  fun open_bracket(): String
    """
    Opening bracket for type argument and type parameter lists in linked
    contexts.
    """

  fun close_bracket(): String
    """
    Closing bracket for type argument and type parameter lists in linked
    contexts.
    """

primitive MkDocsLinkFormat is LinkFormat
  """
  Markdown link format for MkDocs documentation output.

  Produces `[Name](tqfn.md)` cross-references and escaped brackets
  `\[`/`\]` to avoid conflicts with markdown link syntax.
  """

  fun link(name: String, tqfn: String): String =>
    "[" + name + "](" + tqfn + ".md)"

  fun open_bracket(): String => "\\["

  fun close_bracket(): String => "\\]"
