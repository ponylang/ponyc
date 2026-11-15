## Fix http_client IPv6 host handling in redirects and Host header

`http_client` did not bracket IPv6 addresses when writing them into URLs or the `Host` header. Three things broke:

- Every redirect from an IPv6 origin failed with `InvalidLocation`, even when the `Location` was an absolute URL to another host.
- Every request to an IPv6 host sent a malformed `Host` header (`Host: ::1:8080` instead of `Host: [::1]:8080`).
- `Origin` constructed with a bracketed host (`Origin(false, "[::1]", "8080")`) never compared equal to one parsed from a URI, so same-origin redirects from that origin were treated as cross-origin.

IPv6 addresses are now bracketed correctly in redirect URLs and the `Host` header, and an `Origin` constructed with a bracketed IPv6 host (e.g. `"[::1]"`) compares equal to one parsed from a URI.
