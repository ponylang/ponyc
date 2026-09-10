trait _SSLSessionState
  """
  Every SSL operation is dispatched through the current state.
  """
  fun ref receive(ssl: SSL ref, data: ByteSeq): SSLReceiveResult
  fun ref read(ssl: SSL ref, expect: USize): SSLReadResult
  fun ref write(ssl: SSL ref, data: ByteSeq) ?
  fun ref close(ssl: SSL ref)
  fun ref send(ssl: SSL ref): (Array[U8] iso^ | None)
  fun alpn_selected(ssl: SSL box): (ALPNProtocolName | None)
  fun ref dispose(ssl: SSL ref)

class _Handshaking is _SSLSessionState
  fun ref receive(ssl: SSL ref, data: ByteSeq): SSLReceiveResult =>
    ssl._do_receive(data)
    ssl._kick_handshake()

  fun ref read(ssl: SSL ref, expect: USize): SSLReadResult => None

  fun ref write(ssl: SSL ref, data: ByteSeq) ? => error

  fun ref close(ssl: SSL ref) => None

  fun ref send(ssl: SSL ref): (Array[U8] iso^ | None) => ssl._do_send()

  fun alpn_selected(ssl: SSL box): (ALPNProtocolName | None) => None

  fun ref dispose(ssl: SSL ref) =>
    ssl._do_dispose()
    ssl._set_state(_Disposed)

class _Ready is _SSLSessionState
  fun ref receive(ssl: SSL ref, data: ByteSeq): SSLReceiveResult =>
    ssl._do_receive(data)
    SSLAccepted

  fun ref read(ssl: SSL ref, expect: USize): SSLReadResult =>
    ssl._do_read(expect)

  fun ref write(ssl: SSL ref, data: ByteSeq) ? =>
    ssl._do_write(data)?

  fun ref close(ssl: SSL ref) =>
    ssl._do_close_notify()

  fun ref send(ssl: SSL ref): (Array[U8] iso^ | None) => ssl._do_send()

  fun alpn_selected(ssl: SSL box): (ALPNProtocolName | None) =>
    ssl._do_alpn_selected()

  fun ref dispose(ssl: SSL ref) =>
    ssl._do_dispose()
    ssl._set_state(_Disposed)

class _SSLClosing is _SSLSessionState
  """
  Shutdown started but not yet complete. The peer sent `close_notify`;
  we have not responded yet.
  """

  fun ref receive(ssl: SSL ref, data: ByteSeq): SSLReceiveResult =>
    ssl._do_receive(data)
    SSLAccepted

  fun ref read(ssl: SSL ref, expect: USize): SSLReadResult =>
    match \exhaustive\ ssl._do_read(expect)
    | let data: Array[U8] iso => consume data
    | None => SSLClosed
    | SSLClosed => SSLClosed
    | SSLError => SSLError
    | InvalidOperation => _Unreachable(); SSLError
    end

  fun ref write(ssl: SSL ref, data: ByteSeq) ? => error

  fun ref close(ssl: SSL ref) =>
    ssl._do_close_notify()

  fun ref send(ssl: SSL ref): (Array[U8] iso^ | None) => ssl._do_send()

  fun alpn_selected(ssl: SSL box): (ALPNProtocolName | None) =>
    ssl._do_alpn_selected()

  fun ref dispose(ssl: SSL ref) =>
    ssl._do_dispose()
    ssl._set_state(_Disposed)

class _SSLClosed is _SSLSessionState
  """
  Shutdown complete. We have sent our `close_notify`.
  """

  fun ref receive(ssl: SSL ref, data: ByteSeq): SSLReceiveResult =>
    ssl._do_receive(data)
    SSLAccepted

  fun ref read(ssl: SSL ref, expect: USize): SSLReadResult =>
    let result = ssl._do_read(expect)
    ssl._restore_closed_unless_errored(this)
    match \exhaustive\ consume result
    | let data: Array[U8] iso => consume data
    | None => SSLClosed
    | SSLClosed => SSLClosed
    | SSLError => SSLError
    | InvalidOperation => _Unreachable(); SSLError
    end

  fun ref write(ssl: SSL ref, data: ByteSeq) ? => error

  fun ref close(ssl: SSL ref) => None

  fun ref send(ssl: SSL ref): (Array[U8] iso^ | None) => ssl._do_send()

  fun alpn_selected(ssl: SSL box): (ALPNProtocolName | None) =>
    ssl._do_alpn_selected()

  fun ref dispose(ssl: SSL ref) =>
    ssl._do_dispose()
    ssl._set_state(_Disposed)

class _Disposed is _SSLSessionState
  fun ref receive(ssl: SSL ref, data: ByteSeq): SSLReceiveResult =>
    InvalidOperation

  fun ref read(ssl: SSL ref, expect: USize): SSLReadResult => InvalidOperation

  fun ref write(ssl: SSL ref, data: ByteSeq) => None

  fun ref close(ssl: SSL ref) => None

  fun ref send(ssl: SSL ref): (Array[U8] iso^ | None) => None

  fun alpn_selected(ssl: SSL box): (ALPNProtocolName | None) => None

  fun ref dispose(ssl: SSL ref) => None
