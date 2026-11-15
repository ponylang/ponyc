## Fix http_client sending bare CR and LF from Location headers into redirect requests

The response parser in `http_client` kept bare CR, bare LF, and NUL bytes inside parsed header values. A server's `Location` header containing a bare LF could inject extra headers into the redirect request the client sent next, because the LF ended the request line and the bytes after it became headers on the wire.

The parser now replaces bare CR, bare LF, and NUL with SP in header field values, header names, and the status line reason phrase.
