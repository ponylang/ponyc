## Fix http_client request builder appending a second `?` when the path already has a query string

When the path passed to `Request.get` (or any other request method) already contained a query string, calling `.query()` produced a malformed path with two `?` markers. For example, `Request.get("/s?x=1").query("q", "v")` built the path `/s?x=1?q=v` instead of `/s?x=1&q=v`. Builder parameters are now appended with `&` when the path already contains a query string.
