## Fix URI.string() producing ambiguous serializations

`URI.string()` could produce serializations that re-parse as a different URI in two cases, both from RFC 3986 section 3.3.

When a URI had no authority and the path started with `//`, the serialized form was ambiguous — a parser reads the `//` as the authority introducer. `NormalizeURI` and `ResolveURI` could produce such URIs when dot-segment removal turned a path like `/.//evil.com/p` into `//evil.com/p`. `URI.string()` now prefixes `/.` before the path in this case.

```pony
match ParseURI("http:/.//evil.com/p")
| let u: URI val =>
  match NormalizeURI(u)
  | let n: URI val =>
    n.string()
    // was: "http://evil.com/p" (re-parses with host evil.com)
    // now: "http:/.//evil.com/p" (re-parses with no authority)
  end
end
```

When a relative-path reference (no scheme, no authority) had a first path segment containing `:`, the serialized form was ambiguous — a parser reads the text before the colon as a scheme. `NormalizeURI` could produce such URIs when dot-segment removal stripped a leading `./` from a path like `./foo:bar`. `URI.string()` now prefixes `./` before the path in this case.

```pony
match ParseURI("./foo:bar")
| let u: URI val =>
  match NormalizeURI(u)
  | let n: URI val =>
    n.string()
    // was: "foo:bar" (re-parses with scheme "foo")
    // now: "./foo:bar" (re-parses as a relative-path reference)
  end
end
```
