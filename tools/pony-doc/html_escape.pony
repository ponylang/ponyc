primitive HtmlEscape
  """
  Escapes strings for safe insertion into HTML.
  """

  fun content(s: String box): String =>
    """
    Escapes `&`, `<`, and `>` for use in HTML text content.
    """
    let result = recover iso String(s.size()) end
    for byte in s.values() do
      match byte
      | '&' => result.append("&amp;")
      | '<' => result.append("&lt;")
      | '>' => result.append("&gt;")
      else
        result.push(byte)
      end
    end
    consume result

  fun attribute(s: String box): String =>
    """
    Escapes `&`, `<`, `>`, and `"` for use in HTML attribute values.
    """
    let result = recover iso String(s.size()) end
    for byte in s.values() do
      match byte
      | '&' => result.append("&amp;")
      | '<' => result.append("&lt;")
      | '>' => result.append("&gt;")
      | '"' => result.append("&quot;")
      else
        result.push(byte)
      end
    end
    consume result
