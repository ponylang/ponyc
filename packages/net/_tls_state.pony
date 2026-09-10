
primitive _NoTLS
  """
  A plaintext connection. It has never had an SSL session.
  """

class _TLS
  """
  A TLS connection with a live SSL session.
  """
  let session: SSL

  new create(session': SSL) =>
    session = consume session'

primitive _TLSDisposed
  """
  A TLS connection whose SSL session has been disposed. Still TLS — the read
  path must not fall back to treating the read buffer as plaintext — but the
  session is gone and must not be touched.
  """

primitive _TLSFailed
  """
  A TLS connection whose SSL session could not be created. The constructor that
  would have built it cannot fail, so it records this and initialization reports
  it. Still TLS: the connection must never carry application bytes.
  """

type _TLSState is (_NoTLS | _TLS | _TLSDisposed | _TLSFailed)
  """
  Whether a connection is using TLS, and whether its session is usable. Those
  are two questions, and they get different answers once the session is disposed
  or fails to be created, so one variant cannot answer both.

  `_TLS` is the only variant that carries a session, and it exists only while
  that session is alive: `match` is the guard, and nothing else needs to be
  consulted before using what it binds.

  `_NoTLS` means plaintext and nothing else. A connection that asked for TLS and
  did not get it is `_TLSFailed`, never `_NoTLS`, so it can never be taken for a
  connection that never wanted TLS.
  """

primitive _MakeTLS
  """
  Build the TLS state for a connection that wants a session. Creating one can
  fail and the SSL constructors cannot, so the failure becomes `_TLSFailed`
  here rather than in each of them, where it could be written as `_NoTLS` and
  turn a connection that asked for TLS into a plaintext one.

  `start_tls()` does not use this: it reports its own failure to the caller
  rather than storing one.
  """
  fun client(ctx: SSLContext val, hostname: String): _TLSState =>
    try _TLS(ctx.client(hostname)?) else _TLSFailed end

  fun server(ctx: SSLContext val): _TLSState =>
    try _TLS(ctx.server()?) else _TLSFailed end
