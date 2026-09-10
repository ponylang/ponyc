class val TimerToken is Equatable[TimerToken]
  """
  Identifies a timer operation. Returned by `set_timer()` on success and
  delivered to `_on_timer()` when the timer fires.

  Tokens use structural equality based on their ID, which is scoped per
  connection. Applications managing multiple connections should pair tokens
  with connection identity to avoid ambiguity.
  """
  let id: USize

  new val _create(id': USize) =>
    id = id'

  fun eq(that: box->TimerToken): Bool =>
    id == that.id

  fun ne(that: box->TimerToken): Bool =>
    not eq(that)

primitive SetTimerNotOpen
  """
  The connection is not application-level connected. Either the connection
  is not open, or an initial SSL handshake is still in progress (before
  `_on_connected`/`_on_started` has fired).
  """

primitive SetTimerAlreadyActive
  """
  A timer is already active. Cancel it with `cancel_timer()` before setting
  a new one. This prevents silent token invalidation — see `send()` returning
  `SendError` for the same design rationale.
  """

type SetTimerError is
  (SetTimerNotOpen | SetTimerAlreadyActive)
