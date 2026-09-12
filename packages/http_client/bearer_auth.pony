primitive BearerAuth
  """
  Construct an HTTP Bearer token authentication header value.

  Returns a `(name, value)` tuple suitable for passing directly to
  `Headers.set()` or the request builder's `header()` method:
  `("authorization", "Bearer <token>")`.
  """

  fun apply(token: String): (String, String) =>
    """
    Build the Bearer auth header from `token`.
    """
    ("authorization", "Bearer " + token)
