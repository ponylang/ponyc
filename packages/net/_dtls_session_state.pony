trait _DTLSSessionState
  fun ref receive(dtls: DTLS ref, data: ByteSeq): SSLReceiveResult
  fun ref read(dtls: DTLS ref, expect: USize): SSLReadResult
  fun ref write(dtls: DTLS ref, data: ByteSeq) ?
  fun ref close(dtls: DTLS ref)
  fun ref send(dtls: DTLS ref): (Array[U8] iso^ | None)
  fun alpn_selected(dtls: DTLS box): (ALPNProtocolName | None)
  fun ref dispose(dtls: DTLS ref)

class _DTLSHandshaking is _DTLSSessionState
  fun ref receive(dtls: DTLS ref, data: ByteSeq): SSLReceiveResult =>
    dtls._do_receive(data)
    dtls._kick_handshake()

  fun ref read(dtls: DTLS ref, expect: USize): SSLReadResult => None

  fun ref write(dtls: DTLS ref, data: ByteSeq) ? => error

  fun ref close(dtls: DTLS ref) => None

  fun ref send(dtls: DTLS ref): (Array[U8] iso^ | None) => dtls._do_send()

  fun alpn_selected(dtls: DTLS box): (ALPNProtocolName | None) => None

  fun ref dispose(dtls: DTLS ref) =>
    dtls._do_dispose()
    dtls._set_state(_DTLSDisposed)

class _DTLSReady is _DTLSSessionState
  fun ref receive(dtls: DTLS ref, data: ByteSeq): SSLReceiveResult =>
    dtls._do_receive(data)
    SSLAccepted

  fun ref read(dtls: DTLS ref, expect: USize): SSLReadResult =>
    dtls._do_read(expect)

  fun ref write(dtls: DTLS ref, data: ByteSeq) ? =>
    dtls._do_write(data)?

  fun ref close(dtls: DTLS ref) =>
    dtls._do_close_notify()

  fun ref send(dtls: DTLS ref): (Array[U8] iso^ | None) => dtls._do_send()

  fun alpn_selected(dtls: DTLS box): (ALPNProtocolName | None) =>
    dtls._do_alpn_selected()

  fun ref dispose(dtls: DTLS ref) =>
    dtls._do_dispose()
    dtls._set_state(_DTLSDisposed)

class _DTLSClosing is _DTLSSessionState
  """
  The peer sent `close_notify`; we have not responded yet.
  """

  fun ref receive(dtls: DTLS ref, data: ByteSeq): SSLReceiveResult =>
    dtls._do_receive(data)
    SSLAccepted

  fun ref read(dtls: DTLS ref, expect: USize): SSLReadResult =>
    match \exhaustive\ dtls._do_read(expect)
    | let data: Array[U8] iso => consume data
    | None => SSLClosed
    | SSLClosed => SSLClosed
    | SSLError => SSLError
    | InvalidOperation => _Unreachable(); SSLError
    end

  fun ref write(dtls: DTLS ref, data: ByteSeq) ? => error

  fun ref close(dtls: DTLS ref) =>
    dtls._do_close_notify()

  fun ref send(dtls: DTLS ref): (Array[U8] iso^ | None) => dtls._do_send()

  fun alpn_selected(dtls: DTLS box): (ALPNProtocolName | None) =>
    dtls._do_alpn_selected()

  fun ref dispose(dtls: DTLS ref) =>
    dtls._do_dispose()
    dtls._set_state(_DTLSDisposed)

class _DTLSClosed is _DTLSSessionState
  """
  We have sent our `close_notify`.
  """

  fun ref receive(dtls: DTLS ref, data: ByteSeq): SSLReceiveResult =>
    dtls._do_receive(data)
    SSLAccepted

  fun ref read(dtls: DTLS ref, expect: USize): SSLReadResult =>
    let result = dtls._do_read(expect)
    dtls._restore_closed_unless_errored(this)
    match \exhaustive\ consume result
    | let data: Array[U8] iso => consume data
    | None => SSLClosed
    | SSLClosed => SSLClosed
    | SSLError => SSLError
    | InvalidOperation => _Unreachable(); SSLError
    end

  fun ref write(dtls: DTLS ref, data: ByteSeq) ? => error

  fun ref close(dtls: DTLS ref) => None

  fun ref send(dtls: DTLS ref): (Array[U8] iso^ | None) => dtls._do_send()

  fun alpn_selected(dtls: DTLS box): (ALPNProtocolName | None) =>
    dtls._do_alpn_selected()

  fun ref dispose(dtls: DTLS ref) =>
    dtls._do_dispose()
    dtls._set_state(_DTLSDisposed)

class _DTLSDisposed is _DTLSSessionState
  fun ref receive(dtls: DTLS ref, data: ByteSeq): SSLReceiveResult =>
    InvalidOperation

  fun ref read(dtls: DTLS ref, expect: USize): SSLReadResult => InvalidOperation

  fun ref write(dtls: DTLS ref, data: ByteSeq) => None

  fun ref close(dtls: DTLS ref) => None

  fun ref send(dtls: DTLS ref): (Array[U8] iso^ | None) => None

  fun alpn_selected(dtls: DTLS box): (ALPNProtocolName | None) => None

  fun ref dispose(dtls: DTLS ref) => None
