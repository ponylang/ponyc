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
  var _read_buf: Array[U8] iso = recover Array[U8] end

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
      @SSL_ctrl(_ssl, 55, 0, _hostname.null_terminated().cstring())
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

  fun ref read(expect: USize = 0): (Array[U8] iso^ | None) =>
    """
    Returns unencrypted bytes to be passed to the application. If `expect` is
    non-zero, the number of bytes returned will be exactly `expect`. If no data
    (or less than `expect` bytes) is available, this returns None.
    """
    let offset = _read_buf.size()

    var len = if expect > 0 then
      if offset >= expect then
        return _read_buf = recover Array[U8] end
      end

      expect - offset
    else
      1024
    end

    let max = if expect > 0 then expect - offset else USize.max_value() end
    let pending = @SSL_pending[I32](_ssl).usize()

    if pending > 0 then
      if expect > 0 then
        len = len.min(pending)
      else
        len = pending
      end

      _read_buf.undefined(offset + len)
      @SSL_read[I32](_ssl, _read_buf.cstring().usize() + offset, len.i32())
    else
      _read_buf.undefined(offset + len)
      let r = @SSL_read[I32](_ssl, _read_buf.cstring().usize() + offset,
        len.i32())

      if r <= 0 then
        match @SSL_get_error[I32](_ssl, r)
        | 1 | 5 | 6 => _state = SSLError
        end

        _read_buf.truncate(offset)
      else
        _read_buf.truncate(offset + r.usize())
      end
    end

    let ready = if expect == 0 then
      _read_buf.size() > 0
    else
      _read_buf.size() == expect
    end

    if ready then
      _read_buf = recover Array[U8] end
    else
      None
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

  fun ref can_send(): Bool =>
    """
    Returns true if there are encrypted bytes to be passed to the destination.
    """
    @BIO_ctrl_pending[USize](_output) > 0

  fun ref send(): Array[U8] iso^ ? =>
    """
    Returns encrypted bytes to be passed to the destination. Raises an error
    if no data is available.
    """
    let len = @BIO_ctrl_pending[USize](_output)
    if len == 0 then error end

    let buf = recover Array[U8].undefined(len) end
    @BIO_read[I32](_output, buf.cstring(), buf.size().u32())
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
