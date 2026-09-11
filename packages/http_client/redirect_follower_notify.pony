trait ref RedirectFollowerNotify is HTTPClientLifecycleEventReceiver
  """
  Callback trait for actors using RedirectFollower.

  Extends HTTPClientLifecycleEventReceiver with one callback for redirect
  errors. Implement this instead of the base trait when using RedirectFollower.
  """
  fun ref on_redirect_error(err: RedirectError) =>
    """
    Called when a redirect cannot be followed: budget exhausted, Location
    missing or unparseable, or https-to-http downgrade. The 3xx response is
    not delivered through on_response.
    """
    None
