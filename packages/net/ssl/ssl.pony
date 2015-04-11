primitive _SSL
primitive _BIO

class SSL
  """
  An SSL session manages handshakes, encryption and decryption. It is not tied
  to any transport layer.
  """
  var _ssl: Pointer[_SSL] tag
  var _input: Pointer[_BIO] tag
  var _output: Pointer[_BIO] tag
  var _ready: Bool = false
  var _last_read: U64 = 64

  new create(ctx: SSLContext box, server: Bool = false) ? =>
    """
    Create a client or server SSL session from a context.
    """
    _ssl = @SSL_new[Pointer[_SSL]](ctx._context())
    if _ssl.is_null() then error end

    _input = @BIO_new[Pointer[_BIO]](@BIO_s_mem[Pointer[U8]]())
    if _input.is_null() then error end

    _output = @BIO_new[Pointer[_BIO]](@BIO_s_mem[Pointer[U8]]())
    if _output.is_null() then error end

    @SSL_set_bio[None](_ssl, _input, _output)

    if server then
      @SSL_set_accept_state[None](_ssl)
    else
      @SSL_set_connect_state[None](_ssl)
      @SSL_do_handshake[I32](_ssl)
    end

  fun ready(): Bool =>
    """
    Returns true if this SSL session has completed the SSL handshake and send()
    can be called on it with application data.
    """
    _ready

  fun ref read(): Array[U8] iso^ ? =>
    """
    Returns unencrypted bytes to be passed to the application. Raises an error
    if no data is available.
    """
    let pending = @SSL_pending[I32](_ssl)

    if pending > 0 then
      let buf = recover Array[U8].undefined(pending.u64()) end
      @SSL_read[I32](_ssl, buf.cstring(), pending)
      buf
    else
      let len = _last_read
      let buf = recover Array[U8].undefined(len) end
      let r = @SSL_read[I32](_ssl, buf.cstring(), len.i32())

      if r <= 0 then
        error
      end

      if r.u64() == _last_read then
        _last_read = _last_read * 2
      end

      buf.truncate(r.u64())
      buf
    end

  fun ref write(data: Bytes) ? =>
    """
    When application data is sent, add it to the SSL session. Raises an error
    if the handshake is not complete.
    """
    if not _ready then error end

    if data.size() > 0 then
      @SSL_write[I32](_ssl, data.cstring(), data.size().u32())
    end

  fun ref receive(data: Bytes) =>
    """
    When data is received, add it to the SSL session.
    """
    @BIO_write[I32](_input, data.cstring(), data.size().u32())

    if not _ready then
      if @SSL_do_handshake[I32](_ssl) == 1 then
        _ready = true
      end
    end

  fun ref send(): Array[U8] iso^ ? =>
    """
    Returns encrypted bytes to be passed to the destination. Raises an error
    if no data is available.
    """
    let len = @BIO_ctrl_pending[I32](_output)
    if len == 0 then error end

    let buf = recover Array[U8].undefined(len.u64()) end
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
