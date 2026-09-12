class val HTTPRequest
  """
  An HTTP request to be sent by `HTTPClientConnection`.

  Holds the method, path, headers, and optional body. The connection layer
  auto-sets `Host` and `Content-Length` headers during serialization if they
  are not already present — callers only need to set them explicitly when
  overriding the defaults.

  No validation is performed on the request — the client sends whatever the
  caller asks for.
  """
  let method: Method
  let path: String
  let headers: Headers val
  let body: (Array[U8] val | None)

  new val create(
    method': Method,
    path': String,
    headers': Headers val = recover val Headers end,
    body': (Array[U8] val | None) = None)
  =>
    """
    Create an HTTP request with the given method, path, headers, and body.
    """
    method = method'
    path = path'
    headers = headers'
    body = body'
