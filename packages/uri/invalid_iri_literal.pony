primitive InvalidIRILiteral is Stringable
  """
  A non-ASCII character that is not allowed literally in an IRI appears
  unencoded in a URI component.

  RFC 3987 section 2.2 defines which non-ASCII codepoints may appear
  literally: `ucschar` in all components, plus `iprivate` in the query.
  Bidi formatting characters (section 4.1) and invalid UTF-8 byte
  sequences are never allowed literally.
  """
  fun string(): String => "InvalidIRILiteral"
