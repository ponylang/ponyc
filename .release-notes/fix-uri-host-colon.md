## Fix ParseURI accepting hosts containing ':'

`ParseURI` and `ParseURIAuthority` accepted non-bracketed hosts containing `:`, which RFC 3986 forbids in reg-name.

The parsed URI's `string()` dropped the empty port, producing output that `ParseURI` then rejected — `parse(uri.string())` failed for a URI that `ParseURI` itself returned.

```pony
ParseURI("http://a:b:/")
// was: parsed with host "a:b"; string() "http://a:b/" rejected on reparse
// now: rejected with InvalidHost
```
