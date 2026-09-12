primitive FetchInvalidURL
  """
  The URL could not be parsed, has no scheme, has an unsupported scheme,
  or has no host.
  """

primitive FetchConnectionFailed
  """
  A network-level failure: DNS, TCP, SSL, timeout, redirect error, or
  HTTP parse error.
  """

primitive FetchHTTPError
  """
  The server returned a non-2xx status code.
  """

primitive FetchExtractionFailed
  """
  The output directory could not be created or the archive could not be
  decoded.
  """

type FetchErrorKind is
  ( FetchInvalidURL
  | FetchConnectionFailed
  | FetchHTTPError
  | FetchExtractionFailed )

class val FetchError
  """
  A fetch failure with a category and a human-readable detail.
  """
  let kind: FetchErrorKind
  let message: String val

  new val create(kind': FetchErrorKind, message': String val) =>
    kind = kind'
    message = message'
