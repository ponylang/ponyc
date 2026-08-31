primitive HtmlLinkFormat is LinkFormat
  """
  HTML link format for self-contained HTML documentation output.

  Produces `<a href="tqfn.html">Name</a>` cross-references with both
  the name and tqfn HTML-escaped. Brackets are plain `[`/`]` since they
  have no syntactic meaning in HTML.
  """

  fun link(name: String, tqfn: String): String =>
    "<a href=\"" + HtmlEscape.attribute(tqfn) + ".html\">" +
      HtmlEscape.content(name) + "</a>"

  fun open_bracket(): String => "["

  fun close_bracket(): String => "]"
