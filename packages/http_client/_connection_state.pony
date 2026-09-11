use "net"

trait ref _ConnectionState
  """
  Connection lifecycle state.

  Dispatches net events to the appropriate client methods based on
  what operations are valid in each state. Two states: `_Active`
  (connection is open for requests and responses) and `_Closed`
  (all operations are no-ops).
  """

  fun ref on_received(client: HTTPClientConnection ref, data: Array[U8] iso)
    """
    Handle incoming data from the TCP connection.
    """

  fun ref on_closed(client: HTTPClientConnection ref)
    """
    Handle connection close notification.
    """

  fun ref on_throttled(client: HTTPClientConnection ref)
    """
    Handle backpressure applied notification.
    """

  fun ref on_unthrottled(client: HTTPClientConnection ref)
    """
    Handle backpressure released notification.
    """

  fun ref on_idle_timeout(client: HTTPClientConnection ref)
    """
    Handle connection going idle.
    """

  fun ref on_idle_timer_failure(client: HTTPClientConnection ref)
    """
    Handle idle timer ASIO subscription failure.
    """

  fun ref on_timer(client: HTTPClientConnection ref, token: TimerToken)
    """
    Handle one-shot timer firing.
    """

  fun ref on_timer_failure(client: HTTPClientConnection ref)
    """
    Handle user timer ASIO subscription failure.
    """

class ref _Active is _ConnectionState
  """
  Connection is active — sending requests and receiving responses.
  """

  fun ref on_received(client: HTTPClientConnection ref, data: Array[U8] iso) =>
    client._feed_parser(consume data)

  fun ref on_closed(client: HTTPClientConnection ref) =>
    client._handle_closed()

  fun ref on_throttled(client: HTTPClientConnection ref) =>
    client._handle_throttled()

  fun ref on_unthrottled(client: HTTPClientConnection ref) =>
    client._handle_unthrottled()

  fun ref on_idle_timeout(client: HTTPClientConnection ref) =>
    client._handle_idle_timeout()

  fun ref on_idle_timer_failure(client: HTTPClientConnection ref) =>
    client._handle_idle_timer_failure()

  fun ref on_timer(client: HTTPClientConnection ref, token: TimerToken) =>
    client._handle_timer(token)

  fun ref on_timer_failure(client: HTTPClientConnection ref) =>
    client._handle_timer_failure()

class ref _Closed is _ConnectionState
  """
  Connection is closed — all operations are no-ops.
  """

  fun ref on_received(client: HTTPClientConnection ref, data: Array[U8] iso) =>
    None

  fun ref on_closed(client: HTTPClientConnection ref) =>
    None

  fun ref on_throttled(client: HTTPClientConnection ref) =>
    None

  fun ref on_unthrottled(client: HTTPClientConnection ref) =>
    None

  fun ref on_idle_timeout(client: HTTPClientConnection ref) =>
    None

  fun ref on_idle_timer_failure(client: HTTPClientConnection ref) =>
    None

  fun ref on_timer(client: HTTPClientConnection ref, token: TimerToken) =>
    None

  fun ref on_timer_failure(client: HTTPClientConnection ref) =>
    None
