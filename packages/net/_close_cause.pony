primitive _ConnectTimedOut
primitive _ConnectTimerFailed
primitive _TLSAuthFailure
primitive _UnspecifiedCause

type _HardCloseCause is
  ( _ConnectTimedOut
  | _ConnectTimerFailed
  | _TLSAuthFailure
  | _UnspecifiedCause )
  """
  Why a hard close routes to one failure callback rather than another, when the
  connection's state cannot tell them apart. A connect timeout looks like a TCP
  failure from the outside; an auth failure looks like any TLS error. The caller
  that knows passes the distinguishing cause.

  `_UnspecifiedCause` is every other hard close -- the application closed it, or
  the transport failed -- where the state and `_had_inflight` decide the
  callback and there is nothing for the caller to add. It is a variant, not a
  missing one: a `_HardCloseCause` is always one of these, so every hard-close
  path matches it in full.

  This is the internal counterpart of the `ConnectionFailureReason` /
  `TLSFailureReason` a callback receives: the cause is what a caller knows, the
  reason is what the application is told.
  """
