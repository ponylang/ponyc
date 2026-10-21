## Fix ParseURI accepting malformed IPv6 literals

`ParseURI` and `ParseURIAuthority` accepted any bracketed host made only of hex digits, `:` and `.`, without validating the RFC 3986 IPv6address grammar. Malformed literals like `[::1::2]`, `[1:2:3:4:5:6:7:8:9]`, and `[1.2.3.4]` were silently accepted.

```pony
ParseURI("http://[::1::2]/")
// was: parsed with host "[::1::2]"
// now: rejected with InvalidHost
```
