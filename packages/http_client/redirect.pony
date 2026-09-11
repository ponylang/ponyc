use uri = "uri"

class val Redirect
  """
  A validated redirect hop, ready to follow.

  The `Location` header has been resolved against the request URL, security
  rules applied (credentials stripped on a cross-origin hop, `https`-to-`http`
  downgrade refused before a `Redirect` is ever built), and the method and
  body rewritten for the response status. The remaining hop count is already
  decremented for the next hop.

  RedirectFollower uses this internally — application code does not interact
  with it directly. The constructor is private so the security rules cannot
  be bypassed and the remaining hop count cannot be reset.
  """
  let _status: U16
  let _target: uri.URI val
  let _request: HTTPRequest val
  let _remaining: USize

  new val _create(
    status': U16,
    target': uri.URI val,
    request': HTTPRequest val,
    remaining': USize)
  =>
    _status = status'
    _target = target'
    _request = request'
    _remaining = remaining'

  fun status(): U16 =>
    """
    The redirect status code (301, 302, 303, 307, or 308).
    """
    _status

  fun target(): uri.URI val =>
    """
    The resolved absolute URL to follow.
    """
    _target

  fun request(): HTTPRequest val =>
    """
    The request to send on the next hop: the original request with its
    method and body rewritten for the status and its headers stripped for a
    cross-origin hop.
    """
    _request

  fun remaining(): USize =>
    """
    How many redirect hops may still be followed after this one.
    """
    _remaining
