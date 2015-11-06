use "files"

use @SSL_CTX_ctrl[I32](ctx: Pointer[_SSLContext] tag, op: I32, arg: I32,
  parg: Pointer[U8] tag) if windows

use @SSL_CTX_ctrl[I64](ctx: Pointer[_SSLContext] tag, op: I32, arg: I64,
  parg: Pointer[U8] tag) if not windows

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
    _ctx = @SSL_CTX_new[Pointer[_SSLContext]](@SSLv23_method[Pointer[U8]]())

    // set SSL_OP_NO_SSLv2
    @SSL_CTX_ctrl(_ctx, 32, 0x01000000, Pointer[U8])

    // set SSL_OP_NO_SSLv3
    @SSL_CTX_ctrl(_ctx, 32, 0x02000000, Pointer[U8])

    try set_ciphers("ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH") end

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
    let cert' = cert.path
    let key' = key.path

    if
      _ctx.is_null() or
      (cert'.size() == 0) or
      (key'.size() == 0) or
      (@SSL_CTX_use_certificate_chain_file[I32](_ctx, cert'.cstring()) == 0) or
      (@SSL_CTX_use_PrivateKey_file[I32](_ctx, key'.cstring(), I32(1)) == 0) or
      (@SSL_CTX_check_private_key[I32](_ctx) == 0)
    then
      error
    end
    this

  fun ref set_authority(file: (FilePath | None),
                        path: (FilePath | None) = None):
    SSLContext ref^ ?
  =>
    """
    Use a PEM file and/or a directory of PEM files to specify certificate
    authorities. Clients must set this. For servers, it is optional. Use
    None to indicate no file or no path. Raises an error if these
    verify locations aren't valid, or if both are None.
    """
    let extract = lambda(fspec: (FilePath | None)): Pointer[U8] tag
	    => match fspec
	    | let fp: FilePath => fp.path.cstring()
            else
              Pointer[U8]
            end
          end
    let f = extract(file)
    let p = extract(path)

    if
      _ctx.is_null() or
      (f.is_null() and p.is_null()) or
      (@SSL_CTX_load_verify_locations[I32](_ctx, f, p) == 0)
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
      (@SSL_CTX_set_cipher_list[I32](_ctx, ciphers.cstring()) == 0)
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
    Allow TLS v1. Defaults to true.
    """
    if not _ctx.is_null() then
      if state then
        // clear SSL_OP_NO_TLSv1
        @SSL_CTX_ctrl(_ctx, 77, 0x04000000, Pointer[U8])
      else
        // set SSL_OP_NO_TLSv1
        @SSL_CTX_ctrl(_ctx, 32, 0x04000000, Pointer[U8])
      end
    end
    this

  fun ref allow_tls_v1_1(state: Bool): SSLContext ref^ =>
    """
    Allow TLS v1.1. Defaults to true.
    """
    if not _ctx.is_null() then
      if state then
        // clear SSL_OP_NO_TLSv1_1
        @SSL_CTX_ctrl(_ctx, 77, 0x10000000, Pointer[U8])
      else
        // set SSL_OP_NO_TLSv1_1
        @SSL_CTX_ctrl(_ctx, 32, 0x10000000, Pointer[U8])
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
        @SSL_CTX_ctrl(_ctx, 77, 0x08000000, Pointer[U8])
      else
        // set SSL_OP_NO_TLSv1_2
        @SSL_CTX_ctrl(_ctx, 32, 0x08000000, Pointer[U8])
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
