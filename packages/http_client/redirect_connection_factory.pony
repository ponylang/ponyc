use uri = "uri"

interface ref RedirectConnectionFactory
  """
  Creates an HTTPClientConnection to a redirect target. Called by
  RedirectFollower on cross-origin redirects — same-origin redirects reuse
  the existing connection.
  """
  fun ref apply(target: uri.URI val): HTTPClientConnection
    """
    Return a new connection to `target`.
    """
