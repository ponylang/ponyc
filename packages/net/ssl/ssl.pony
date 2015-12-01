use "net"

use @SSL_ctrl[ILong](ssl: Pointer[_SSL], op: I32, arg: ILong,
  parg: Pointer[None])

primitive _SSL
primitive _BIO

primitive SSLHandshake
primitive SSLAuthFail
primitive SSLReady
primitive SSLError

type SSLState is (SSLHandshake | SSLAuthFail | SSLReady | SSLError)

class SSL
  """
  An SSL session manages handshakes, encryption and decryption. It is not tied
  to any transport layer.
  """
  let _hostname: String
  var _ssl: Pointer[_SSL]
  var _input: Pointer[_BIO] tag
  var _output: Pointer[_BIO] tag
  var _state: SSLState = SSLHandshake
  var _last_read: USize = 64

  new _create(ctx: Pointer[_SSLContext] tag, server: Bool, verify: Bool,
    hostname: String = "") ?
  =>
    """
    Create a client or server SSL session from a context.
    """
    if ctx.is_null() then error end
    _hostname = hostname

    _ssl = @SSL_new[Pointer[_SSL]](ctx)
    if _ssl.is_null() then error end

    let mode = if verify then I32(3) else I32(0) end
    @SSL_set_verify[None](_ssl, mode, Pointer[U8])

    _input = @BIO_new[Pointer[_BIO]](@BIO_s_mem[Pointer[U8]]())
    if _input.is_null() then error end

    _output = @BIO_new[Pointer[_BIO]](@BIO_s_mem[Pointer[U8]]())
    if _output.is_null() then error end

    @SSL_set_bio[None](_ssl, _input, _output)

    if (_hostname.size() > 0) and
      not DNS.is_ip4(_hostname) and
      not DNS.is_ip6(_hostname)
    then
      // SSL_set_tlsext_host_name
      @SSL_ctrl(_ssl, 55, 0, _hostname.cstring())
    end

    if server then
      @SSL_set_accept_state[None](_ssl)
    else
      @SSL_set_connect_state[None](_ssl)
      @SSL_do_handshake[I32](_ssl)
    end

  fun state(): SSLState =>
    """
    Returns the SSL session state.
    """
    _state

  fun ref read(): Array[U8] iso^ ? =>
    """
    Returns unencrypted bytes to be passed to the application. Raises an error
    if no data is available.
    """
    let pending = @SSL_pending[I32](_ssl)

    if pending > 0 then
      let buf = recover Array[U8].undefined(pending.usize()) end
      @SSL_read[I32](_ssl, buf.cstring(), pending)
      buf
    else
      let len = _last_read
      let buf = recover Array[U8].undefined(len) end
      let r = @SSL_read[I32](_ssl, buf.cstring(), len.i32())

      if r <= 0 then
        match @SSL_get_error[I32](_ssl, r)
        | 1 | 5 | 6 => _state = SSLError
        end
        error
      end

      if r.usize() == _last_read then
        _last_read = _last_read * 2
      end

      buf.truncate(r.usize())
      buf
    end

  fun ref write(data: ByteSeq) ? =>
    """
    When application data is sent, add it to the SSL session. Raises an error
    if the handshake is not complete.
    """
    if _state isnt SSLReady then error end

    if data.size() > 0 then
      @SSL_write[I32](_ssl, data.cstring(), data.size().u32())
    end

  fun ref receive(data: ByteSeq) =>
    """
    When data is received, add it to the SSL session.
    """
    @BIO_write[I32](_input, data.cstring(), data.size().u32())

    if _state is SSLHandshake then
      let r = @SSL_do_handshake[I32](_ssl)

      if r > 0 then
        _verify_hostname()
      else
        match @SSL_get_error[I32](_ssl, r)
        | 1 => _state = SSLAuthFail
        | 5 | 6 => _state = SSLError
        end
      end
    end

  fun ref send(): Array[U8] val ? =>
    """
    Returns encrypted bytes to be passed to the destination. Raises an error
    if no data is available.
    """
    let len = @BIO_ctrl_pending[I32](_output)
    if len == 0 then error end

    let buf = recover Array[U8].undefined(len.usize()) end
    @BIO_read[I32](_output, buf.cstring(), len)
    buf

  fun ref dispose() =>
    """
    Dispose of the session.
    """
    if not _ssl.is_null() then
      @SSL_free[None](_ssl)
      _ssl = Pointer[_SSL]
    end

  fun _final() =>
    """
    Dispose of the session.
    """
    if not _ssl.is_null() then
      @SSL_free[None](_ssl)
    end

  fun ref _verify_hostname() =>
    """
    Verify that the certificate is valid for the given hostname.
    """
    if _hostname.size() > 0 then
      let cert = @SSL_get_peer_certificate[Pointer[X509]](_ssl)
      let ok = X509.valid_for_host(cert, _hostname)

      if not cert.is_null() then
        @X509_free[None](cert)
      end

      if not ok then
        _state = SSLAuthFail
        return
      end
    end

    _state = SSLReady
