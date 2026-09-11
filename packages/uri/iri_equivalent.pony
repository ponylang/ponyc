primitive IRIEquivalent
  """
  Test whether two IRIs (or an IRI and a URI) are equivalent under
  RFC 3987 normalization.

  Normalizes both inputs with `NormalizeIRI` and compares them
  structurally. This detects equivalence across IRI and URI forms —
  for example, a literal `é` and its percent-encoded form `%C3%A9`
  are equivalent.

  Returns `InvalidPercentEncoding` if either input contains a malformed
  percent-encoded sequence.
  """
  fun apply(a: URI val, b: URI val)
    : (Bool | InvalidPercentEncoding val)
  =>
    let norm_a =
      match \exhaustive\ NormalizeIRI(a)
      | let u: URI val => u
      | let e: InvalidPercentEncoding val => return e
      end
    let norm_b =
      match \exhaustive\ NormalizeIRI(b)
      | let u: URI val => u
      | let e: InvalidPercentEncoding val => return e
      end
    norm_a == norm_b
