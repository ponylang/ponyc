primitive NormalizeIRI
  """
  Normalize an IRI per RFC 3987 section 5.3.

  Applies `NormalizeURI` (RFC 3986 section 6 syntax-based and scheme-based
  normalization), then converts the result to IRI form with `URIToIRI` to
  decode `ucschar` sequences back to literal UTF-8.

  NFC normalization is not applied — input is assumed to be NFC-normalized
  already (RFC 3987 section 5.3.2.4).

  Returns `InvalidPercentEncoding` if any component contains a malformed
  percent-encoded sequence.
  """
  fun apply(iri: URI val): (URI val | InvalidPercentEncoding val) =>
    match \exhaustive\ NormalizeURI(iri)
    | let normalized: URI val => URIToIRI(normalized)
    | let e: InvalidPercentEncoding val => e
    end
