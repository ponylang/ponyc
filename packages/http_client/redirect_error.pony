primitive TooManyRedirects
  """
  The redirect budget was exhausted before the chain resolved to a
  non-redirect response.
  """

primitive MissingLocation
  """
  A redirect response carried no `Location` header.
  """

primitive InvalidLocation
  """
  A redirect response's `Location` header did not resolve to a usable
  `http` or `https` URL.
  """

primitive InsecureRedirect
  """
  A redirect would downgrade an `https` connection to `http`.
  """

type RedirectError is
  ( TooManyRedirects
  | MissingLocation
  | InvalidLocation
  | InsecureRedirect )
  """
  Why a redirect could not be followed, delivered via
  `on_redirect_error()`. Each member documents its own case.
  """
