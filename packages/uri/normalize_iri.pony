primitive NormalizeIRI
  """
  Normalize an IRI per RFC 3987 section 5.3.

  Applies `NormalizeURI` (RFC 3986 section 6 syntax-based and scheme-based
  normalization), then converts the result to IRI form with `URIToIRI` to
  decode `ucschar` sequences back to literal UTF-8.

  NFC normalization is not applied — input is assumed to be NFC-normalized
  already (RFC 3987 section 5.3.2.4).

  Returns `InvalidIRILiteral` if any component contains a literal non-ASCII
  character that is not allowed in an IRI. Allowed literally: `ucschar` in
  all components, plus `iprivate` in the query. Bidi formatting characters
  and invalid UTF-8 are never allowed. Returns `InvalidPercentEncoding` if
  any component contains a malformed percent-encoded sequence.
  """
  fun apply(iri: URI val)
    : (URI val | InvalidPercentEncoding val | InvalidIRILiteral val)
  =>
    if not _validate_components(iri) then
      return InvalidIRILiteral
    end
    match \exhaustive\ NormalizeURI(iri)
    | let normalized: URI val => URIToIRI(normalized)
    | let e: InvalidPercentEncoding val => e
    end

  fun _validate_components(iri: URI val): Bool =>
    match \exhaustive\ iri.authority
    | let a: URIAuthority =>
      match \exhaustive\ a.userinfo
      | let u: String =>
        if not _ValidateIRILiterals.is_valid(u, false) then
          return false
        end
      | None => None
      end
      if not _ValidateIRILiterals.is_valid(a.host, false) then
        return false
      end
    | None => None
    end
    if not _ValidateIRILiterals.is_valid(iri.path, false) then
      return false
    end
    match \exhaustive\ iri.query
    | let q: String =>
      if not _ValidateIRILiterals.is_valid(q, true) then
        return false
      end
    | None => None
    end
    match \exhaustive\ iri.fragment
    | let f: String =>
      if not _ValidateIRILiterals.is_valid(f, false) then
        return false
      end
    | None => None
    end
    true
