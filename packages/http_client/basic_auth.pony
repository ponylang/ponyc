use "encode/base64"

primitive BasicAuth
  """
  Construct an HTTP Basic Authentication header value.

  Returns a `(name, value)` tuple suitable for passing directly to
  `Headers.set()` or the request builder's `header()` method:
  `("authorization", "Basic <base64(username:password)>")`.
  """

  fun apply(username: String, password: String): (String, String) =>
    """
    Build the Basic auth header from `username` and `password`.

    The credentials are encoded as `base64(username:password)` per RFC 7617.
    """
    let credentials: String val = username + ":" + password
    let encoded: String val = Base64.encode(credentials)
    ("authorization", "Basic " + encoded)
