class DTLS
  """
  A DTLS session manages handshakes, encryption and decryption. It is not tied
  to any transport layer.

  DTLS is the datagram variant of TLS. It provides the same authentication and
  encryption guarantees over unreliable transports like UDP. This type is
  separate from `SSL` because DTLS and TLS sessions are not interchangeable:
  a TLS context cannot create a DTLS session and vice versa.
  """
  let _hostname: String
  let _verify: Bool
  // Nothing reads this. `SSL_new` takes a reference on the `SSL_CTX`, so the
  // `SSL_CTX` outlives a `DTLSContext` the caller drops while this session is
  // alive. Holding the context here keeps it, and the ALPN resolver it handed
  // to OpenSSL, alive for as long as the session can drive a handshake.
  let _context: DTLSContext
  var _ssl: Pointer[_SSL] = Pointer[_SSL]
  var _input: Pointer[_BIO] tag = Pointer[_BIO]
  var _output: Pointer[_BIO] tag = Pointer[_BIO]
  var _state: _DTLSSessionState = _DTLSHandshaking
  var _read_buf: Array[U8] iso = []

  new _create(
    context: DTLSContext val,
    server: Bool,
    verify: Bool,
    hostname: String = "")
    ?
  =>
    """
    Create a client or server DTLS session from a context.
    """
    let ctx = context._ssl_ctx()
    if ctx.is_null() then error end
    _context = context
    _hostname = hostname
    _verify = verify

    _ssl = @SSL_new(ctx)
    if _ssl.is_null() then error end

    let mode = if verify then I32(3) else I32(0) end
    @SSL_set_verify(_ssl, mode, Pointer[None])

    _input = @BIO_new(@BIO_s_mem())
    if _input.is_null() then error end

    _output = @BIO_new(@BIO_s_mem())
    if _output.is_null() then
      @BIO_free(_input)
      _input = Pointer[_BIO]
      error
    end

    @SSL_set_bio(_ssl, _input, _output)

    if
      (_hostname.size() > 0) and
        not DNS.is_ip4(_hostname) and
        not DNS.is_ip6(_hostname)
    then
      // SSL_set_tlsext_host_name
      @SSL_ctrl(_ssl, 55, 0, _hostname.cstring())
    end

    if server then
      @SSL_set_accept_state(_ssl)
    else
      @SSL_set_connect_state(_ssl)
      _kick_handshake()
    end

  fun box alpn_selected(): (ALPNProtocolName | None) =>
    """
    The protocol identifier negotiated via ALPN, or `None` when no protocol
    has been selected.
    """
    _state.alpn_selected(this)

  fun ref close() =>
    """
    Send `close_notify` to the peer, initiating an orderly shutdown.
    After calling this, drain `send` to deliver the encrypted `close_notify`
    bytes to the transport.
    """
    _state.close(this)

  fun ref read(expect: USize = 0): SSLReadResult =>
    """
    Returns unencrypted bytes to be passed to the application, `None` when
    no data is available yet, `SSLClosed` when the peer sent `close_notify`,
    or `SSLError` on a protocol or I/O error.

    When `expect` is non-zero, buffers internally until at least `expect`
    bytes are available, then returns everything it holds.
    """
    _state.read(this, expect)

  fun ref write(data: ByteSeq) ? =>
    """
    Encrypt application data for sending. Raises an error when the session
    is not ready for application data or when encryption fails.
    """
    _state.write(this, data)?

  fun ref receive(data: ByteSeq): SSLReceiveResult =>
    """
    Feed encrypted data from the transport into the session.

    Returns what happened: `SSLAccepted` when data was accepted with nothing
    else to report, `SSLReady` when the handshake completed, `SSLAuthFail`
    when the peer's certificate was rejected, `SSLError` on failure, or
    `InvalidOperation` when the session is no longer operational.
    """
    _state.receive(this, data)

  fun ref send(): (Array[U8] iso^ | None) =>
    """
    Returns encrypted bytes to be passed to the destination, or `None` when
    there is nothing to send.
    """
    _state.send(this)

  fun ref dispose() =>
    """
    Dispose of the session.
    """
    _state.dispose(this)

  fun _final() =>
    if not _ssl.is_null() then
      @SSL_free(_ssl)
    end

  fun ref _set_state(new_state: _DTLSSessionState) =>
    _state = new_state

  fun ref _drain_read_buf(expect: USize): (Array[U8] iso^ | None) =>
    if (expect > 0) and (_read_buf.size() >= expect) then
      return _read_buf = []
    end
    None

  fun ref _do_receive(data: ByteSeq) =>
    let total = data.size()
    if total > 0 then
      let max_chunk = I32.max_value().usize()
      var offset: USize = 0
      while offset < total do
        let chunk = (total - offset).min(max_chunk)
        @BIO_write(_input, data.cpointer(offset), chunk.i32())
        offset = offset + chunk
      end
    end

  fun ref _kick_handshake(): SSLReceiveResult =>
    @ERR_clear_error()
    let r = @SSL_do_handshake(_ssl)

    if r > 0 then
      _verify_hostname()
    else
      match @SSL_get_error(_ssl, r)
      | _SSLErrorCode.ssl() | _SSLErrorCode.syscall() =>
        if _peer_auth_failed() then
          _state = _DTLSAuthFailed
          SSLAuthFail
        else
          _state = _DTLSErrored
          SSLError
        end
      | _SSLErrorCode.zero_return() =>
        _state = _DTLSErrored
        SSLError
      | _SSLErrorCode.want_read() =>
        SSLAccepted
      else
        _Unreachable()
        SSLError
      end
    end

  fun ref _do_read(expect: USize): SSLReadResult =>
    let offset = _read_buf.size()

    var len =
      if expect > 0 then
        if offset >= expect then
          return _read_buf = []
        end

        expect - offset
      else
        1024
      end

    let pending = @SSL_pending(_ssl).usize()

    if pending > 0 then
      len = if expect > 0 then len.min(pending) else pending end
    end

    len = len.min(I32.max_value().usize())
    _read_buf.undefined(offset + len)
    @ERR_clear_error()
    let r = @SSL_read(_ssl, _read_buf.cpointer(offset), len.i32())

    let filled = if r > 0 then r.usize() else 0 end
    _read_buf.truncate(offset + filled)

    if r <= 0 then
      match @SSL_get_error(_ssl, r)
      | _SSLErrorCode.ssl()
      | _SSLErrorCode.syscall() =>
        _state = _DTLSErrored
        return SSLError
      | _SSLErrorCode.zero_return() =>
        _state = _DTLSClosing
        if _read_buf.size() > 0 then
          return _read_buf = []
        end
        return SSLClosed
      | _SSLErrorCode.want_read() =>
        return None
      else
        _Unreachable()
        return None
      end
    end

    let ready =
      if expect == 0 then
        _read_buf.size() > 0
      else
        _read_buf.size() == expect
      end

    if ready then
      _read_buf = []
    else
      ifdef "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" then
        if @BIO_ctrl_pending(_input) > 0 then
          _do_read(expect)
        elseif @SSL_has_pending(_ssl) == 1 then
          _do_read(expect)
        end
      elseif "libressl" then
        if @BIO_ctrl_pending(_input) > 0 then
          _do_read(expect)
        end
      else
        compile_error "You must select an SSL version to use."
      end
    end

  fun ref _do_write(data: ByteSeq) ? =>
    let total = data.size()
    if total > 0 then
      let max_chunk = I32.max_value().usize()
      var offset: USize = 0
      while offset < total do
        let chunk = (total - offset).min(max_chunk)
        @ERR_clear_error()
        let r = @SSL_write(_ssl, data.cpointer(offset), chunk.i32())
        if r <= 0 then
          match @SSL_get_error(_ssl, r)
          | _SSLErrorCode.ssl()
          | _SSLErrorCode.syscall() =>
            _state = _DTLSErrored
          | _SSLErrorCode.zero_return() =>
            _state = _DTLSClosing
          | _SSLErrorCode.want_read() =>
            None
          else
            _Unreachable()
          end
          error
        end
        offset = offset + chunk
      end
    end

  fun ref _do_close_notify() =>
    @ERR_clear_error()
    let r = @SSL_shutdown(_ssl)
    if r < 0 then
      let err = @SSL_get_error(_ssl, r)
      if (err == _SSLErrorCode.ssl()) or (err == _SSLErrorCode.syscall()) then
        _state = _DTLSErrored
        return
      else
        _Unreachable()
      end
    end
    _state = _DTLSClosed

  fun ref _do_send(): (Array[U8] iso^ | None) =>
    let pending = @BIO_ctrl_pending(_output)
    if pending == 0 then return None end

    let len = pending.min(I32.max_value().usize())
    let buf = recover Array[U8] .> undefined(len) end
    let r = @BIO_read(_output, buf.cpointer(), len.i32())
    if r <= 0 then return None end
    buf.truncate(r.usize())
    buf

  fun box _do_alpn_selected(): (ALPNProtocolName | None) =>
    var ptr: Pointer[U8] iso = recover Pointer[U8] end
    var len = U32(0)
    ifdef
      "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
    then
      @SSL_get0_alpn_selected(_ssl, addressof ptr, addressof len)
    else
      compile_error "You must select an SSL version to use."
    end

    if ptr.is_null() then None
    else
      recover val String.copy_cpointer(consume ptr, USize.from[U32](len)) end
    end

  fun ref _do_dispose() =>
    if not _ssl.is_null() then
      @SSL_free(_ssl)
      _ssl = Pointer[_SSL]
      _input = Pointer[_BIO]
      _output = Pointer[_BIO]
    end

  fun ref _peer_auth_failed(): Bool =>
    """
    Whether the handshake failure the caller just got from `SSL_do_handshake`
    was this session rejecting its peer's certificate.

    True for a chain that did not verify, for a peer that sent no certificate
    when one was required, and for a peer that presented a certificate but
    could not prove it holds the matching key. False for a peer whose
    certificate would not parse: the failure happens before chain verification,
    so it is not distinguishable from one that had nothing to do with a
    certificate.

    Called for both `SSL_ERROR_SSL` and `SSL_ERROR_SYSCALL`. A callback that
    runs inside `SSL_do_handshake` can push an entry onto the thread's error
    queue, changing `SSL_get_error` from one to the other without changing what
    actually failed. Routing both through this method keeps the reported state
    consistent.

    Callers must have checked that `_ssl` is not null, and must arrive with the
    thread's error queue as `SSL_do_handshake` left it. A peer that sent no
    certificate leaves its reason only on that queue, so an OpenSSL call that
    clears the queue in between loses it and this returns false.
    """
    if not _verify then return false end

    if @SSL_get_verify_result(_ssl) != _X509VerifyResult.ok() then
      return true
    end

    var code = @ERR_get_error()
    while code != 0 do
      if
        (_ERRLibrary.of(code) == _ERRLibrary.ssl()) and
          (_ERRReason.of(code) ==
            _ERRReason.peer_did_not_return_a_certificate())
      then
        return true
      end
      code = @ERR_get_error()
    end

    let cert =
      ifdef "openssl_3.0.x" or "openssl_4.0.x" then
        @SSL_get1_peer_certificate(_ssl)
      elseif "openssl_1.1.x" or "libressl" then
        @SSL_get_peer_certificate(_ssl)
      else
        compile_error "You must select an SSL version to use."
      end

    if not cert.is_null() then
      @X509_free(cert)
      return true
    end

    false

  fun ref _verify_hostname(): SSLReceiveResult =>
    if _verify and (_hostname.size() > 0) then
      let cert =
        ifdef "openssl_3.0.x" or "openssl_4.0.x" then
          @SSL_get1_peer_certificate(_ssl)
        elseif "openssl_1.1.x" or "libressl" then
          @SSL_get_peer_certificate(_ssl)
        else
          compile_error "You must select an SSL version to use."
        end
      let ok = X509.valid_for_host(cert, _hostname)

      if not cert.is_null() then
        @X509_free(cert)
      end

      if not ok then
        _state = _DTLSAuthFailed
        return SSLAuthFail
      end
    end

    _state = _DTLSReady
    SSLReady

  fun ref _restore_closed_unless_errored(closed: _DTLSClosed) =>
    match _state
    | let _: _DTLSErrored => None
    else _state = closed
    end
