"""
# URI Parsing and Resolution (RFC 3986)

This package parses URI-references into structured components and resolves
relative references against base URIs per RFC 3986.

## Entry Points

Use `ParseURI` for standard URI-references (absolute URIs and relative
references — covers origin-form, absolute-form, and asterisk-form HTTP
request-targets):

```pony
match ParseURI("/index.html?page=1")
| let u: URI val => // u.path, u.query, etc.
| let e: URIParseError val => // handle error
end
```

Use `ResolveURI` to resolve a relative reference against a base URI
(RFC 3986 section 5):

```pony
match (ParseURI("http://example.com/a/b"), ParseURI("../c"))
| (let base: URI val, let ref': URI val) =>
  match ResolveURI(base, ref')
  | let target: URI val => // target.string() == "http://example.com/a/c"
  | let e: ResolveURIError val => // base was not absolute
  end
end
```

Use `ParseURIAuthority` for HTTP CONNECT authority-form targets
(`host:port` without scheme or `//`):

```pony
match ParseURIAuthority("example.com:443")
| let a: URIAuthority val => // a.host, a.port
| let e: URIParseError val => // handle error
end
```

Use `NormalizeURI` to apply RFC 3986 section 6 normalization (case,
percent-encoding, dot segments, default port removal, empty path):

```pony
match ParseURI("HTTP://Example.COM:80/%7Euser/a/../b")
| let u: URI val =>
  match NormalizeURI(u)
  | let n: URI val => // n.string() == "http://example.com/~user/b"
  | let e: InvalidPercentEncoding val => // malformed percent-encoding
  end
end
```

Use `URIEquivalent` to test whether two URIs are equivalent under
normalization:

```pony
match (ParseURI("HTTP://Example.COM:80/path"),
  ParseURI("http://example.com/path"))
| (let a: URI val, let b: URI val) =>
  match URIEquivalent(a, b)
  | let eq: Bool => // eq == true
  end
end
```

Use `URIBuilder` to construct a URI from raw (unencoded) components with
automatic percent-encoding, or to modify an existing URI:

```pony
match URIBuilder
  .set_scheme("https")
  .set_host("example.com")
  .set_path("/api/users")
  .add_query_param("name", "Jane Doe")
  .build()
| let u: URI val =>
  // u.string() == "https://example.com/api/users?name=Jane%20Doe"
| let e: URIBuildError val =>
  // handle error
end
```

To modify an existing URI, start from `URIBuilder.from(uri)` and change
only the components you need:

```pony
match ParseURI("https://example.com/old?x=1")
| let u: URI val =>
  match URIBuilder.from(u)
    .set_path("/new")
    .add_query_param("y", "2")
    .build()
  | let modified: URI val => // "https://example.com/new?x=1&y=2"
  end
end
```

For query parameter access, `URI.query_params()` is the simplest path —
it returns a `FormURLEncoded` collection with `get()`, `get_all()`, and
`contains()` methods for key-based lookup. Use `ParseFormURLEncoded`
directly on the `query` field when you need to distinguish "no query"
from "invalid percent-encoding." `ParseFormURLEncoded` also works on
HTTP POST request bodies — it parses any `application/x-www-form-urlencoded`
string. Use `PercentDecode`/`PercentEncode` for encoding operations,
`PathSegments` for decoded path segment access, and `RemoveDotSegments`
for standalone path normalization.

For URI template expansion (RFC 6570), use the `uri/template` subpackage.

## IRI Support (RFC 3987)

`ParseURI`, `ResolveURI`, `PercentDecode`, and `PercentEncode` handle IRIs
natively — `ParseURI` only looks for ASCII structural delimiters, so
non-ASCII bytes pass through correctly, and `URI` stores components as
`String` (UTF-8).

The IRI-specific primitives handle conversion between IRI and URI forms:

Use `IRIToURI` to convert an IRI to a URI by percent-encoding all non-ASCII
bytes. Use `URIToIRI` for the reverse — selectively decoding percent-encoded
UTF-8 sequences that are valid `ucschar` (or `iprivate` in query).

Use `IRIPercentEncode` instead of `PercentEncode` when constructing IRIs
from unencoded text — it preserves `ucschar` codepoints as literal UTF-8
while applying the same ASCII encoding rules.

Use `NormalizeIRI` for IRI-aware normalization (applies `NormalizeURI` then
decodes `ucschar` sequences back to literal UTF-8). Use `IRIEquivalent`
to test equivalence across IRI and URI forms.

"""

class val URI is (Stringable & Equatable[URI])
  """
  A parsed RFC 3986 URI-reference.

  Components are stored in their percent-encoded form as they appeared in
  the input. Delimiter characters (`:` after scheme, `//` before authority,
  `?` before query, `#` before fragment) are stripped — use `string()` to
  reconstruct the full URI with delimiters.

  `string()` reconstruction follows RFC 3986 section 5.3. Components that
  are `None` are omitted entirely (no delimiter). A component that is an
  empty `String` (present but empty) includes its delimiter — e.g.,
  `query` of `""` produces a trailing `?` with no value. This distinction
  allows faithful reconstruction of inputs like `http://example.com/path?`
  (empty query present, `query = ""`) vs. `http://example.com/path`
  (no query at all, `query = None`).

  Equality is structural: two URIs are equal when all their stored
  (percent-encoded) components are equal. No normalization is applied
  before comparison. Use `URIEquivalent` for normalization-aware
  comparison (RFC 3986 section 6).
  """
  let scheme: (String | None)
  let authority: (URIAuthority | None)
  let path: String
  let query: (String | None)
  let fragment: (String | None)

  new val create(
    scheme': (String | None),
    authority': (URIAuthority | None),
    path': String,
    query': (String | None),
    fragment': (String | None))
  =>
    """
    Build a URI from pre-encoded components.

    All string values must already be percent-encoded as appropriate for
    their component — no encoding is applied here. Most callers will obtain
    a `URI` from `ParseURI` rather than constructing one directly.
    """
    scheme = scheme'
    authority = authority'
    path = path'
    query = query'
    fragment = fragment'

  fun string(): String iso^ =>
    """
    Reconstruct the full URI string per RFC 3986 section 5.3.
    """
    let out = recover iso String end
    match scheme
    | let s: String => out.append(s); out.push(':')
    end
    match authority
    | let a: URIAuthority =>
      out.append("//")
      out.append(a.string())
    end
    out.append(path)
    match query
    | let q: String => out.push('?'); out.append(q)
    end
    match fragment
    | let f: String => out.push('#'); out.append(f)
    end
    out

  fun query_params(): (FormURLEncoded val | None) =>
    """
    Parse the query string into a `FormURLEncoded` collection.

    Returns the parsed parameters if the query is present and decodes
    successfully, or `None` if no query is present or if the query
    contains invalid percent-encoding. For fine-grained error handling
    (distinguishing "no query" from "decode failure"), use
    `ParseFormURLEncoded` directly on the `query` field.
    """
    match \exhaustive\ query
    | let q: String val =>
      match \exhaustive\ ParseFormURLEncoded(q)
      | let params: FormURLEncoded val => params
      | let _: InvalidPercentEncoding => None
      end
    | None => None
    end

  fun eq(that: URI box): Bool =>
    """
    Structural equality on percent-encoded components without normalization.

    Use `URIEquivalent` for normalization-aware comparison.
    """
    let scheme_eq =
      match (scheme, that.scheme)
      | (None, None) => true
      | (let a: String, let b: String) => a == b
      else
        false
      end
    let authority_eq =
      match (authority, that.authority)
      | (None, None) => true
      | (let a: URIAuthority, let b: URIAuthority) => a == b
      else
        false
      end
    let query_eq =
      match (query, that.query)
      | (None, None) => true
      | (let a: String, let b: String) => a == b
      else
        false
      end
    let fragment_eq =
      match (fragment, that.fragment)
      | (None, None) => true
      | (let a: String, let b: String) => a == b
      else
        false
      end
    scheme_eq and authority_eq and (path == that.path) and
      query_eq and fragment_eq
