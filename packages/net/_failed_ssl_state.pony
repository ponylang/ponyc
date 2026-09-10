trait _FailedSSLState is _SSLSessionState
  """
  Shared defaults for `_AuthFailed` and `_Errored`. The session has failed
  but the OpenSSL handle is still live — outgoing error or rejection bytes
  may need sending via `send`.
  """

  fun ref read(ssl: SSL ref, expect: USize): SSLReadResult =>
    match \exhaustive\ ssl._drain_read_buf(expect)
    | let data: Array[U8] iso => consume data
    | None => SSLError
    end

  fun ref write(ssl: SSL ref, data: ByteSeq) ? => error

  fun ref close(ssl: SSL ref) => None

  fun ref send(ssl: SSL ref): (Array[U8] iso^ | None) => ssl._do_send()

  fun alpn_selected(ssl: SSL box): (ALPNProtocolName | None) => None

  fun ref dispose(ssl: SSL ref) =>
    ssl._do_dispose()
    ssl._set_state(_Disposed)

class _AuthFailed is _FailedSSLState
  fun ref receive(ssl: SSL ref, data: ByteSeq): SSLReceiveResult =>
    SSLAuthFail

class _Errored is _FailedSSLState
  fun ref receive(ssl: SSL ref, data: ByteSeq): SSLReceiveResult =>
    SSLError
