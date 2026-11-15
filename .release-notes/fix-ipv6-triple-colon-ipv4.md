## Fix `ParseURIAuthority` accepting `:::` before an embedded IPv4 address

`ParseURIAuthority`, `ParseURI`, and `URIBuilder.build` accepted IPv6 literals where `:::` preceded an embedded IPv4 address — `[:::1.2.3.4]` and `[1:::1.2.3.4]`. These are invalid: `:::` never appears in an IPv6 address. The same literals without the embedded IPv4 address (`[:::1]`, `[1:::2]`) were already rejected.
