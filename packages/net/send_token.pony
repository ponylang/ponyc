class val SendToken is Equatable[SendToken]
  """
  Identifies a single `send()`. Delivered to `_on_send_accepted()` when the
  send is accepted, then delivered exactly once more: to `_on_sent()` when
  that send's bytes have been handed to the OS, or to `_on_send_failed()` if
  the connection is lost or hard-closed first. A graceful `close()` sends
  what's still queued, so those sends get `_on_sent`.

  "Handed to the OS" means written to the kernel send buffer, not received by
  the peer. End-to-end delivery is an application concern -- use your own
  acknowledgements if you need it.

  Tokens use structural equality based on their ID, which is scoped per
  connection. Applications managing multiple connections should pair tokens
  with connection identity to avoid ambiguity.
  """
  let id: USize

  new val _create(id': USize) =>
    id = id'

  fun eq(that: box->SendToken): Bool =>
    id == that.id

  fun ne(that: box->SendToken): Bool =>
    not eq(that)

primitive SendAccepted
  """
  The send was accepted. Its `SendToken` arrived at `_on_send_accepted()`,
  which fired before `send()` returned.
  """

primitive SendErrorNotConnected
  """
  The connection is not yet established or has already been closed.
  """

primitive SendErrorNotWriteable
  """
  The socket is not writeable. This happens during backpressure (a previous
  send is still pending) or when the socket's send buffer is full.
  Wait for `_on_unthrottled` before retrying.
  """

type SendError is
  (SendErrorNotConnected | SendErrorNotWriteable)

type SendResult is (SendAccepted | SendError)
  """
  What `send()` returns: `SendAccepted`, or a `SendError` saying why the
  connection would not take the data.
  """
