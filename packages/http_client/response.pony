class val Response
  """
  Parsed HTTP response metadata (status line and headers).

  Delivered via `on_response()` after the status line and all headers have
  been parsed. For responses with a body, `on_body_chunk()` calls follow.
  For responses without a body (HEAD, 204, 304), `on_response_complete()`
  is called immediately after.
  """
  let version: Version
  let status: U16
  let reason: String val
  let headers: Headers val

  new val create(
    version': Version,
    status': U16,
    reason': String val,
    headers': Headers val)
  =>
    version = version'
    status = status'
    reason = reason'
    headers = headers'
