trait _DTLSFailedState is _DTLSSessionState
  """
  Shared defaults for `_DTLSAuthFailed` and `_DTLSErrored`. The session has
  failed but the OpenSSL handle is still live.
  """

  fun ref read(dtls: DTLS ref, expect: USize): SSLReadResult =>
    match \exhaustive\ dtls._drain_read_buf(expect)
    | let data: Array[U8] iso => consume data
    | None => SSLError
    end

  fun ref write(dtls: DTLS ref, data: ByteSeq) ? => error

  fun ref close(dtls: DTLS ref) => None

  fun ref send(dtls: DTLS ref): (Array[U8] iso^ | None) => dtls._do_send()

  fun alpn_selected(dtls: DTLS box): (ALPNProtocolName | None) => None

  fun ref dispose(dtls: DTLS ref) =>
    dtls._do_dispose()
    dtls._set_state(_DTLSDisposed)

class _DTLSAuthFailed is _DTLSFailedState
  fun ref receive(dtls: DTLS ref, data: ByteSeq): SSLReceiveResult =>
    SSLAuthFail

class _DTLSErrored is _DTLSFailedState
  fun ref receive(dtls: DTLS ref, data: ByteSeq): SSLReceiveResult =>
    SSLError
