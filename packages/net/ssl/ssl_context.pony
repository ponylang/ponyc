use "files"

use @SSL_CTX_ctrl[ILong](ctx: Pointer[_SSLContext] tag, op: I32, arg: ILong,
  parg: Pointer[None])

use @BIO_new_mem_buf[Pointer[_BIO]](buf: Pointer[U8] tag, len: USize)
use @BIO_ctrl[ILong](bp: Pointer[_BIO] tag, cmd: ISize, larg: ILong,
  parg: Pointer[U8] tag)
use @BIO_free[USize](bp: Pointer[_BIO] tag)

use @PEM_read_bio_X509[Pointer[X509]](bp: Pointer[_BIO] tag,
  x: Pointer[Pointer[X509]] tag,
  cb: Pointer[_PEMPasswdCB] tag, u: Pointer[U8] tag)
use @PEM_read_bio_X509_AUX[Pointer[X509]](bp: Pointer[_BIO] tag,
  x: Pointer[Pointer[X509]] tag,
  cb: Pointer[_PEMPasswdCB] tag, u: Pointer[U8] tag)
use @PEM_read_bio_PrivateKey[Pointer[_EVPPKey]](bp: Pointer[_BIO] tag,
  x: Pointer[Pointer[_EVPPKey]],
  cb: Pointer[_PEMPasswdCB] tag, u: Pointer[U8] tag)

use @X509_free[None](x509: Pointer[X509] tag)
use @EVP_PKEY_free[None](key: Pointer[_EVPPKey] tag)

primitive _SSLContext
primitive _PEMPasswdCB
primitive _EVPPKey

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

  fun ref set_cert(cert: (FilePath | Array[U8] val),
    key: (FilePath | Array[U8] val)) ?
  =>
    """
    Set the public certificate and private key to be used by this SSL
    connection.

    The cert is a PEM certificate chain; the first certificate is the public
    certificate for this connection and any remaining certificates should
    represent the certificate authorities in the signing chain of that
    certificate. The key is a PEM private key. Both may be either a path to a
    file containing the PEM data or a memory buffer containing the PEM data.

    Servers must set this. For clients, it is optional.
    """
    // Set the certificate
    if not ((try cert as FilePath else None end) is None) then
      _set_cert_file(cert as FilePath)
    elseif not ((try cert as Array[U8] val else None end) is None) then
      _set_cert_buffer(cert as Array[U8] val)
    else
      error
    end
    // Set the key
    if not ((try key as FilePath else None end) is None) then
      _set_key_file(key as FilePath)
    elseif not ((try key as Array[U8] val else None end) is None) then
      _set_key_buffer(key as Array[U8] val)
    else
      error
    end
    // Validate the certificate and key
    _check_certificate_and_key()
    this

  fun ref _set_cert_file(cert: FilePath) ? =>
    if
      _ctx.is_null() or
      (cert.path.size() == 0) or
      (0 == @SSL_CTX_use_certificate_chain_file[I32](_ctx,
        cert.path.cstring()))
    then
      error
    end

  fun ref _set_key_file(key: FilePath) ? =>
    if
      _ctx.is_null() or
      (key.path.size() == 0) or
      (0 == @SSL_CTX_use_PrivateKey_file[I32](_ctx,
        key.path.cstring(), I32(1)))
    then
      error
    end

  fun ref _set_cert_buffer(cert: Array[U8] val) ? =>
    // Place the certificate into buffer and insert it into the context.
    if _ctx.is_null() or (cert.size() == 0) then
      error
    end
    // get DEFAULT_PASSWORD_CALLBACK
    let cb = @ponyint_ssl_ctx_accessor[Pointer[_PEMPasswdCB]](_ctx, ISize(0))
    // get DEFAULT_PASSWORD_CALLBACK_USERDATA
    let cb_data = @ponyint_ssl_ctx_accessor[Pointer[U8]](_ctx, ISize(1))
    // get BIO's containing certificate and key
    let cert_buf = @BIO_new_mem_buf(cert.cpointer(), cert.size())
    if cert_buf.is_null() then
      error
    end
    // Set BIO_CTRL_SET_CLOSE to BIO_NOCLOSE
    @BIO_ctrl(cert_buf, ISize(9), ILong(0), Pointer[U8])
    try
      _buffer_set_cert_bio(cert_buf, cb, cb_data)
    else
      @BIO_free(cert_buf)
      error
    end
    @BIO_free(cert_buf)

  fun ref _set_key_buffer(key: Array[U8] val) ? =>
    if _ctx.is_null() or (key.size() == 0)
    then
      error
    end
    // get DEFAULT_PASSWORD_CALLBACK
    let cb = @ponyint_ssl_ctx_accessor[Pointer[_PEMPasswdCB]](_ctx, ISize(0))
    // get DEFAULT_PASSWORD_CALLBACK_USERDATA
    let cb_data = @ponyint_ssl_ctx_accessor[Pointer[U8]](_ctx, ISize(1))
    // get BIO's containing certificate and key
    let key_buf = @BIO_new_mem_buf(key.cpointer(), key.size())
    if key_buf.is_null() then
      error
    end
    // Set BIO_CTRL_SET_CLOSE to BIO_NOCLOSE
    @BIO_ctrl(key_buf, ISize(9), ILong(0), Pointer[U8])
    try
      _buffer_set_privatekey_bio(key_buf, cb, cb_data)
    else
      @BIO_free(key_buf)
      error
    end
    @BIO_free(key_buf)

  fun ref _buffer_set_cert_bio(buf: Pointer[_BIO] tag,
    cb: Pointer[_PEMPasswdCB] tag, cb_data: Pointer[U8] tag) ?
  =>
    // Read the X509 certificate and any following CA certificates and install
    // them into the context.
    let x509 = @PEM_read_bio_X509_AUX(buf, Pointer[Pointer[X509]], cb, cb_data)
    if x509.is_null() then
      error
    end
    if (0 == @SSL_CTX_use_certificate[USize](_ctx, x509)) or
      (@ERR_peek_error[ULong]() != 0)
    then
      @X509_free(x509)
      error
    end
    @X509_free(x509)
    _buffer_set_chain_certs_bio(buf, cb, cb_data)

  fun ref _buffer_set_chain_certs_bio(buf: Pointer[_BIO] tag,
    cb: Pointer[_PEMPasswdCB] tag, cb_data: Pointer[U8] tag) ?
  =>
    // Add additional CA certificates from the buffer. Later versions of OpenSSL
    // provide better interfaces for this.
    @ponyint_ssl_ctx_clear_extra_certs[None](_ctx)
    var ca = @PEM_read_bio_X509(buf, Pointer[Pointer[X509]], cb, cb_data)
    while not ca.is_null() do
      // OpenSSL later vesions (untested):
      //   if 0 == @SSL_CTX_add0_chain_cert[I32](_ctx, ca) then
      // Set SSL_CTRL_EXTRA_CHAIN_CERT
      if 0 == @SSL_CTX_ctrl(_ctx, 14, 0, ca) then
        @X509_free(ca)
        error
      end
      // Don't free ca if it was added to the chain
      ca = @PEM_read_bio_X509(buf, Pointer[Pointer[X509]], cb, cb_data)
    end

  fun ref _buffer_set_privatekey_bio(buf: Pointer[_BIO] tag,
    cb: Pointer[_PEMPasswdCB] tag, cb_data: Pointer[U8] tag) ?
  =>
    let key = @PEM_read_bio_PrivateKey(buf, Pointer[Pointer[_EVPPKey]],
      cb, cb_data)
    if key.is_null() then
      error
    end
    if 0 == @SSL_CTX_use_PrivateKey[USize](_ctx, key) then
      @EVP_PKEY_free(key)
      error
    end

  fun ref _check_certificate_and_key() ? =>
    if (_ctx.is_null()) or
      (0 == @SSL_CTX_check_private_key[I32](_ctx))
    then
      error
    end

  fun ref set_authority(file: (FilePath | None),
    path: (FilePath | None) = None) ?
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

  fun ref set_ciphers(ciphers: String) ? =>
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

  fun ref set_client_verify(state: Bool) =>
    """
    Set to true to require verification. Defaults to true.
    """
    _client_verify = state

  fun ref set_server_verify(state: Bool) =>
    """
    Set to true to require verification. Defaults to false.
    """
    _server_verify = state

  fun ref set_verify_depth(depth: U32) =>
    """
    Set the verify depth. Defaults to 6.
    """
    if not _ctx.is_null() then
      @SSL_CTX_set_verify_depth[None](_ctx, depth)
    end

  fun ref allow_tls_v1(state: Bool) =>
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

  fun ref allow_tls_v1_1(state: Bool) =>
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

  fun ref allow_tls_v1_2(state: Bool) =>
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
