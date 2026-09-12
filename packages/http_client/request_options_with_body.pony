interface ref RequestOptionsWithBody
  """
  Builder options for HTTP methods that support a request body.

  Extends the common options (headers, query params, auth) with methods for
  setting the request body. Available for POST, PUT, PATCH, DELETE, and
  OPTIONS.

  After calling a body method, the return type narrows to `RequestOptions`
  which does not expose body methods — this prevents accidentally setting the
  body twice. The body is always optional: calling `build()` without setting
  a body produces a request with no body.
  """

  fun ref header(hdr_name: String, hdr_value: String):
    RequestOptionsWithBody ref
    """
    Add a header to the request.
    """

  fun ref query(key: String, value: String): RequestOptionsWithBody ref
    """
    Add a query parameter. Parameters are percent-encoded in `build()`.
    """

  fun ref basic_auth(username: String, password: String):
    RequestOptionsWithBody ref
    """
    Set the Authorization header using HTTP Basic authentication.
    """

  fun ref bearer_auth(token: String): RequestOptionsWithBody ref
    """
    Set the Authorization header using a Bearer token.
    """

  fun ref body(data: Array[U8] val): RequestOptions ref
    """
    Set the request body as raw bytes.
    """

  fun ref json_body(data: String): RequestOptions ref
    """
    Set the request body to `data` and add `Content-Type: application/json`.
    """

  fun ref form_body(params: Array[(String, String)] val): RequestOptions ref
    """
    URL-encode `params` via `FormEncoder`, set as body, and add
    `Content-Type: application/x-www-form-urlencoded`.
    """

  fun ref multipart_body(form: MultipartFormData): RequestOptions ref
    """
    Set the request body from a `MultipartFormData` builder.

    Sets `Content-Type` to `multipart/form-data` with the boundary.
    """

  fun ref build(): HTTPRequest val
    """
    Build the final `HTTPRequest val`.
    """
