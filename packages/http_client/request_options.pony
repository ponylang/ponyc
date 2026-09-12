interface ref RequestOptions
  """
  Builder options available for all HTTP request methods.

  Provides methods for setting headers, query parameters, and authentication.
  Use `build()` to produce the final `HTTPRequest val`.

  Methods that accept a body (POST, PUT, PATCH, DELETE, OPTIONS) return
  `RequestOptionsWithBody` instead, which extends this interface with
  body-setting methods.
  """

  fun ref header(hdr_name: String, hdr_value: String): RequestOptions ref
    """
    Add a header to the request.
    """

  fun ref query(key: String, value: String): RequestOptions ref
    """
    Add a query parameter. Parameters are percent-encoded in `build()`.
    """

  fun ref basic_auth(username: String, password: String): RequestOptions ref
    """
    Set the Authorization header using HTTP Basic authentication.
    """

  fun ref bearer_auth(token: String): RequestOptions ref
    """
    Set the Authorization header using a Bearer token.
    """

  fun ref build(): HTTPRequest val
    """
    Build the final `HTTPRequest val`.
    """
