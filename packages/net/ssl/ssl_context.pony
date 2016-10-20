use "files"

use @SSL_CTX_ctrl[ILong](ctx: Pointer[_SSLContext] tag, op: I32, arg: ILong,
  parg: Pointer[None])

primitive _SSLContext

class val SSLContext
  """
  An SSL context is used to create SSL sessions.
  """
  var _ctx: Pointer[_SSLContext] tag
  var _client_verify: Bool = true
  var _server_verify: Bool = false

  new create() =>
    """
    Create an SSL context.
    """
    _ctx = @SSL_CTX_new[Pointer[_SSLContext]](@SSLv23_method[Pointer[None]]())

    // set SSL_OP_NO_SSLv2
    @SSL_CTX_ctrl(_ctx, 32, 0x01000000, Pointer[None])

    // set SSL_OP_NO_SSLv3
    @SSL_CTX_ctrl(_ctx, 32, 0x02000000, Pointer[None])

    // set SSL_OP_NO_TLSv1
    @SSL_CTX_ctrl(_ctx, 32, 0x04000000, Pointer[None])

    // set SSL_OP_NO_TLSv1_1
    @SSL_CTX_ctrl(_ctx, 32, 0x10000000, Pointer[None])

    try
      set_ciphers(
        "ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA256")
    end

  fun client(hostname: String = ""): SSL iso^ ? =>
    """
    Create a client-side SSL session. If a hostname is supplied, the server
    side certificate must be valid for that hostname.
    """
    let ctx = _ctx
    let verify = _client_verify
    recover SSL._create(ctx, false, verify, hostname) end

  fun server(): SSL iso^ ? =>
    """
    Create a server-side SSL session.
    """
    let ctx = _ctx
    let verify = _server_verify
    recover SSL._create(ctx, true, verify) end

  fun ref set_cert(cert: FilePath, key: FilePath): SSLContext ref^ ? =>
    """
    The cert file is a PEM certificate chain. The key file is a private key.
    Servers must set this. For clients, it is optional.
    """
    if
      _ctx.is_null() or
      (cert.path.size() == 0) or
      (key.path.size() == 0) or
      (0 == @SSL_CTX_use_certificate_chain_file[I32](
        _ctx, cert.path.cstring())) or
      (0 == @SSL_CTX_use_PrivateKey_file[I32](
        _ctx, key.path.cstring(), I32(1))) or
      (0 == @SSL_CTX_check_private_key[I32](_ctx))
    then
      error
    end
    this

  fun ref set_authority(file: (FilePath | None),
    path: (FilePath | None) = None): SSLContext ref^ ?
  =>
    """
    Use a PEM file and/or a directory of PEM files to specify certificate
    authorities. Clients must set this. For servers, it is optional. Use None
    to indicate no file or no path. Raises an error if these verify locations
    aren't valid, or if both are None.
    """
    let fs = try (file as FilePath).path else "" end
    let ps = try (path as FilePath).path else "" end

    let f = if fs.size() > 0 then fs.cstring() else Pointer[U8] end
    let p = if ps.size() > 0 then ps.cstring() else Pointer[U8] end

    if
      _ctx.is_null() or
      (f.is_null() and p.is_null()) or
      (0 == @SSL_CTX_load_verify_locations[I32](_ctx, f, p))
    then
      error
    end
    this

  fun ref set_ciphers(ciphers: String): SSLContext ref^ ? =>
    """
    Set the accepted ciphers. This replaces the existing list. Raises an error
    if the cipher list is invalid.
    """
    if
      _ctx.is_null() or
      (0 == @SSL_CTX_set_cipher_list[I32](_ctx,
        ciphers.cstring()))
    then
      error
    end
    this

  fun ref set_client_verify(state: Bool): SSLContext ref^ =>
    """
    Set to true to require verification. Defaults to true.
    """
    _client_verify = state
    this

  fun ref set_server_verify(state: Bool): SSLContext ref^ =>
    """
    Set to true to require verification. Defaults to false.
    """
    _server_verify = state
    this

  fun ref set_verify_depth(depth: U32): SSLContext ref^ =>
    """
    Set the verify depth. Defaults to 6.
    """
    if not _ctx.is_null() then
      @SSL_CTX_set_verify_depth[None](_ctx, depth)
    end
    this

  fun ref allow_tls_v1(state: Bool): SSLContext ref^ =>
    """
    Allow TLS v1. Defaults to false.
    """
    if not _ctx.is_null() then
      if state then
        // clear SSL_OP_NO_TLSv1
        @SSL_CTX_ctrl(_ctx, 77, 0x04000000, Pointer[None])
      else
        // set SSL_OP_NO_TLSv1
        @SSL_CTX_ctrl(_ctx, 32, 0x04000000, Pointer[None])
      end
    end
    this

  fun ref allow_tls_v1_1(state: Bool): SSLContext ref^ =>
    """
    Allow TLS v1.1. Defaults to false.
    """
    if not _ctx.is_null() then
      if state then
        // clear SSL_OP_NO_TLSv1_1
        @SSL_CTX_ctrl(_ctx, 77, 0x10000000, Pointer[None])
      else
        // set SSL_OP_NO_TLSv1_1
        @SSL_CTX_ctrl(_ctx, 32, 0x10000000, Pointer[None])
      end
    end
    this

  fun ref allow_tls_v1_2(state: Bool): SSLContext ref^ =>
    """
    Allow TLS v1.2. Defaults to true.
    """
    if not _ctx.is_null() then
      if state then
        // clear SSL_OP_NO_TLSv1_2
        @SSL_CTX_ctrl(_ctx, 77, 0x08000000, Pointer[None])
      else
        // set SSL_OP_NO_TLSv1_2
        @SSL_CTX_ctrl(_ctx, 32, 0x08000000, Pointer[None])
      end
    end
    this

  fun ref dispose() =>
    """
    Free the SSL context.
    """
    if not _ctx.is_null() then
      @SSL_CTX_free[None](_ctx)
      _ctx = Pointer[_SSLContext]
    end

  fun _final() =>
    """
    Free the SSL context.
    """
    if not _ctx.is_null() then
      @SSL_CTX_free[None](_ctx)
    end
