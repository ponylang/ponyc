class val HTTPResponse
  """
  A buffered HTTP response containing status, headers, and the complete body.

  `HTTPResponse` is the result of collecting streaming response callbacks into a
  single object. Use `ResponseCollector` to accumulate `on_response()` and
  `on_body_chunk()` data, then call `build()` to produce an `HTTPResponse`.

  The entire body is held in memory as a contiguous `Array[U8] val`. For large
  responses where memory is a concern, use the raw `on_body_chunk()` callbacks
  directly instead of collecting.
  """
  let version: Version
  let status: U16
  let reason: String val
  let headers: Headers val
  let body: Array[U8] val

  new val create(
    version': Version,
    status': U16,
    reason': String val,
    headers': Headers val,
    body': Array[U8] val)
  =>
    """
    Create a buffered HTTP response with all fields.
    """
    version = version'
    status = status'
    reason = reason'
    headers = headers'
    body = body'
