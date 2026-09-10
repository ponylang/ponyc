use @SSL_ctrl[ILong](
  ssl: Pointer[_SSL],
  op: I32,
  arg: ILong,
  parg: Pointer[None])
use @SSL_new[Pointer[_SSL]](ctx: Pointer[_SSLContext] tag)
use @SSL_free[None](ssl: Pointer[_SSL] tag)
use @SSL_set_verify[None](ssl: Pointer[_SSL], mode: I32, cb: Pointer[None])
use @BIO_s_mem[Pointer[_BIOMethod]]()
use @BIO_new[Pointer[_BIO]](typ: Pointer[_BIOMethod])
use @BIO_free[I32](bio: Pointer[_BIO] tag)
use @SSL_set_bio[None](
  ssl: Pointer[_SSL],
  rbio: Pointer[_BIO] tag,
  wbio: Pointer[_BIO] tag)
use @SSL_set_accept_state[None](ssl: Pointer[_SSL])
use @SSL_set_connect_state[None](ssl: Pointer[_SSL])
use @SSL_do_handshake[I32](ssl: Pointer[_SSL])
use @SSL_get0_alpn_selected[None](
  ssl: Pointer[_SSL] tag,
  data: Pointer[Pointer[U8] iso],
  len: Pointer[U32])
  if "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
use @SSL_pending[I32](ssl: Pointer[_SSL])
use @SSL_read[I32](ssl: Pointer[_SSL], buf: Pointer[U8] tag, len: I32)
use @SSL_write[I32](ssl: Pointer[_SSL], buf: Pointer[U8] tag, len: I32)
use @BIO_read[I32](bio: Pointer[_BIO] tag, buf: Pointer[U8] tag, len: I32)
use @BIO_write[I32](bio: Pointer[_BIO] tag, buf: Pointer[U8] tag, len: I32)
use @SSL_shutdown[I32](ssl: Pointer[_SSL])
use @SSL_get_error[I32](ssl: Pointer[_SSL], ret: I32)
use @SSL_get_verify_result[ILong](ssl: Pointer[_SSL] tag)
use @ERR_clear_error[None]()
use @ERR_get_error[ULong]()
use @BIO_ctrl_pending[USize](bio: Pointer[_BIO] tag)
use @SSL_has_pending[I32](ssl: Pointer[_SSL])
  if "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x"
use @SSL_get_peer_certificate[Pointer[X509]](
  ssl: Pointer[_SSL]) if "openssl_1.1.x" or "libressl"
use @SSL_get1_peer_certificate[Pointer[X509]](
  ssl: Pointer[_SSL]) if "openssl_3.0.x" or "openssl_4.0.x"

primitive _SSL
primitive _BIO
primitive _BIOMethod

primitive _SSLErrorCode
  """
  `SSL_get_error` results, from `openssl/ssl.h`.
  """
  fun ssl(): I32 => 1
  fun want_read(): I32 => 2
  fun syscall(): I32 => 5
  fun zero_return(): I32 => 6

primitive _X509VerifyResult
  """
  `SSL_get_verify_result` results, from `openssl/x509_vfy.h`.
  """
  fun ok(): ILong => 0

primitive _ERRLibrary
  """
  Which OpenSSL library raised an error, and the one value this package
  compares that against. `of` takes the field from where each backend's
  `ERR_GET_LIB` does, because OpenSSL 3.0 moved it.

  Any OpenSSL library can put an entry on the thread's error queue, so `ssl()`
  is one of several libraries `of` can name. OpenSSL 3.0 also packs system
  errors into the same word, under a flag that neither `_ERRLibrary.of` nor
  `_ERRReason.of` reads. A system-flagged word decodes to a library that is not
  `ssl()`, so it cannot match.
  """
  fun of(code: ULong): ULong =>
    ifdef "openssl_3.0.x" or "openssl_4.0.x" then
      (code >> 23) and 0xFF
    elseif "openssl_1.1.x" or "libressl" then
      (code >> 24) and 0xFF
    else
      compile_error "You must select an SSL version to use."
    end

  fun ssl(): ULong => 20

primitive _ERRReason
  """
  Why an OpenSSL library raised an error, and the one value this package
  compares that against. `of` masks the field the way each backend's
  `ERR_GET_REASON` does, because OpenSSL 3.0 widened it.

  A reason number means one thing in the library that raised it and something
  else in another, so compare it only after `_ERRLibrary.of` has named that
  library. 199 is `SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE` in libssl and
  `ASN1_R_UNKNOWN_SIGNATURE_ALGORITHM` in libcrypto's ASN.1.
  """
  fun of(code: ULong): ULong =>
    ifdef "openssl_3.0.x" or "openssl_4.0.x" then
      code and 0x7FFFFF
    elseif "openssl_1.1.x" or "libressl" then
      code and 0xFFF
    else
      compile_error "You must select an SSL version to use."
    end

  fun peer_did_not_return_a_certificate(): ULong => 199

class SSL
  """
  An SSL session manages handshakes, encryption and decryption. It is not tied
  to any transport layer.
  """
  let _hostname: String
  let _verify: Bool
  // Nothing reads this. `SSL_new` takes a reference on the `SSL_CTX`, so the
  // `SSL_CTX` outlives an `SSLContext` the caller drops while this session is
  // alive. Holding the context here keeps it, and the ALPN resolver it handed
  // to OpenSSL, alive for as long as the session can drive a handshake.
  let _context: SSLContext
  var _ssl: Pointer[_SSL] = Pointer[_SSL]
  var _input: Pointer[_BIO] tag = Pointer[_BIO]
  var _output: Pointer[_BIO] tag = Pointer[_BIO]
  var _state: _SSLSessionState = _Handshaking
  var _read_buf: Array[U8] iso = []

  new _create(
    context: SSLContext val,
    server: Bool,
    verify: Bool,
    hostname: String = "")
    ?
  =>
    """
    Create a client or server SSL session from a context. The session holds the
    context, so the context and the ALPN resolver it installed with OpenSSL stay
    alive for as long as the session can handshake.
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
      // `SSL_set_bio` below is what hands the BIOs to the session, and it has
      // not run, so the `SSL_free` in `_final` will not free `_input`.
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
    Send `close_notify` to the peer, initiating an orderly TLS shutdown.
    After calling this, drain `send` to deliver the encrypted `close_notify`
    bytes to the transport. Does nothing when the session is not ready or
    has already been closed.

    A graceful socket close finishes queued writes first, then calls `close`,
    drains `send`, and calls `dispose` when done. A hard close skips `close`
    and calls `dispose` directly.
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

  fun ref _set_state(new_state: _SSLSessionState) =>
    _state = new_state

  fun ref _drain_read_buf(expect: USize): (Array[U8] iso^ | None) =>
    """
    Return buffered bytes without calling `SSL_read`. When `expect` is
    non-zero and enough bytes are buffered, return them all. With `expect`
    zero, return `None` — a no-expect read only returns data that was
    freshly decrypted in the same call, and a failed session decrypts
    nothing.
    """
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
    """
    Run one step of the TLS handshake and transition state accordingly. Shared
    between the constructor (client path) and `_Handshaking.receive`.
    """
    @ERR_clear_error()
    let r = @SSL_do_handshake(_ssl)

    if r > 0 then
      _verify_hostname()
    else
      match @SSL_get_error(_ssl, r)
      | _SSLErrorCode.ssl() | _SSLErrorCode.syscall() =>
        if _peer_auth_failed() then
          _state = _AuthFailed
          SSLAuthFail
        else
          _state = _Errored
          SSLError
        end
      | _SSLErrorCode.zero_return() =>
        _state = _Errored
        SSLError
      | _SSLErrorCode.want_read() =>
        SSLAccepted
      else
        _Unreachable()
        SSLError
      end
    end

  fun ref _do_read(expect: USize): SSLReadResult =>
    """
    `SSL_read` with expect-mode buffering and `SSL_pending` retry. Sets state
    to `_SSLClosing` on `zero_return` and to `_Errored` on fatal error.
    """
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
        _state = _Errored
        return SSLError
      | _SSLErrorCode.zero_return() =>
        _state = _SSLClosing
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
          // SSL has buffered data that BIO_ctrl_pending cannot see.
          // pony-lint: allow style/line-length
          // https://mta.openssl.org/pipermail/openssl-users/2017-January/005110.html
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
    """
    `SSL_write` with chunking. Sets state to `_SSLClosing` on `zero_return` and
    to `_Errored` on fatal error.
    """
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
            _state = _Errored
          | _SSLErrorCode.zero_return() =>
            _state = _SSLClosing
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
    """
    Send `close_notify` via `SSL_shutdown`. Transitions to `_SSLClosed` on
    success, `_Errored` on fatal error.
    """
    @ERR_clear_error()
    let r = @SSL_shutdown(_ssl)
    if r < 0 then
      let err = @SSL_get_error(_ssl, r)
      if (err == _SSLErrorCode.ssl()) or (err == _SSLErrorCode.syscall()) then
        _state = _Errored
        return
      else
        _Unreachable()
      end
    end
    _state = _SSLClosed

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
      // `_create` handed both BIOs to the session with `SSL_set_bio`, so
      // `SSL_free` frees all three. Nulling `_ssl` is what keeps `_final`
      // from double-freeing. Null the BIOs as well, so any code path that
      // ever bypasses the state machine finds a null pointer instead of freed
      // memory.
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

    // A failure inside libcrypto puts its own entry on the queue before libssl
    // puts this one there, so the entry to look at is not always the first.
    // Taking entries off is safe because every `SSL_*` call clears the queue
    // before it runs.
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

    // A peer that presented a certificate it could not prove it holds still
    // has that certificate stored in the session. The chain verified, or the
    // check above would have caught it, and no "no certificate" reason was on
    // the queue. A non-null peer certificate at this point means the handshake
    // failed after the peer's credentials were received — an authentication
    // failure, whatever error code the backend reported.
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
        _state = _AuthFailed
        return SSLAuthFail
      end
    end

    _state = _Ready
    SSLReady

  fun ref _restore_closed_unless_errored(closed: _SSLClosed) =>
    match _state
    | let _: _Errored => None
    else _state = closed
    end
