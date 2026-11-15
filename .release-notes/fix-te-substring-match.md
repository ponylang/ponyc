## Fix http_client treating partial Transfer-Encoding matches as chunked

The http_client response parser matched "chunked" as a substring of the Transfer-Encoding header value. A value like `notchunked` or `chunked, gzip` was incorrectly parsed as chunked transfer encoding. The parser now correctly identifies `chunked` only when it appears as the final transfer coding, per RFC 9112 §6.3.

HTTP/1.0 responses with a Transfer-Encoding header are now handled as close-delimited, per RFC 9112 §6.1. Previously, `Transfer-Encoding: chunked` on an HTTP/1.0 response would attempt chunked decoding.

When Transfer-Encoding is present but the final coding is not `chunked`, Content-Length is now correctly ignored, per RFC 9112 §6.3 item 3.
