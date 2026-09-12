primitive URIEquivalent
  """
  Test whether two URIs are equivalent under RFC 3986 normalization.

  Normalizes both URIs via `NormalizeURI` (syntax-based and scheme-based)
  and compares them with structural equality. This catches equivalences
  that raw string or structural comparison would miss — for example,
  `HTTP://Example.COM:80/path` and `http://example.com/path` are
  equivalent.

  Returns `InvalidPercentEncoding` if either URI contains a malformed
  percent-encoded sequence.
  """
  fun apply(a: URI val, b: URI val)
    : (Bool | InvalidPercentEncoding val)
  =>
    let norm_a =
      match \exhaustive\ NormalizeURI(a)
      | let u: URI val => u
      | let e: InvalidPercentEncoding val => return e
      end
    let norm_b =
      match \exhaustive\ NormalizeURI(b)
      | let u: URI val => u
      | let e: InvalidPercentEncoding val => return e
      end
    norm_a == norm_b
