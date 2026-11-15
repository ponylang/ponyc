## Fix http_client sending Content-Length alongside a caller-set Transfer-Encoding

When a request had a body and the caller set `Transfer-Encoding`, the library still added a `Content-Length` header. RFC 9112 section 6.2 forbids sending both. The automatic `Content-Length` is now suppressed when `Transfer-Encoding` is already present.
