"""
Redirect following example.

Connects to `httpbin.org` over HTTPS and requests `/redirect/3`, which returns
three chained 302 redirects before a final 200. The actor's callbacks see only
the final response — `RedirectFollower` handles the hops internally.

Demonstrates `RedirectFollower` wrapping an `HTTPClientConnection`,
`RedirectFollowerNotify` for the `on_redirect_error` callback,
`RedirectConnectionFactory` as a lambda for cross-origin hops, and
`_http_client_connection()` delegating to `_http.connection()`.
"""
