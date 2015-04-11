primitive _SSLContext

class SSLContext
  """
  An SSL context is used to create SSL sessions.
  """
  var _ctx: Pointer[_SSLContext] tag

  new create() ? =>
    """
    Creates an SSL context.
    """
    _ctx = @SSL_CTX_new[Pointer[_SSLContext]](@SSLv23_method[Pointer[U8]]())
    if _ctx.is_null() then error end

    set_ciphers("ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH")
    set_verify(true)

  fun ref set_cert(public: String, private: String): SSLContext ? =>
    """
    The public file is a PEM certificate chain. The private file is a DSA key.
    Servers must set this. For clients, it is optional.
    """
    if @SSL_CTX_use_certificate_chain_file[I32](_ctx, public.cstring()) == 0
    then error end

    if @SSL_CTX_use_PrivateKey_file[I32](_ctx, private.cstring(), I32(1)) == 0
    then error end

    if @SSL_CTX_check_private_key[I32](_ctx) == 0 then error end
    this

  fun ref set_ca_file(file: String): SSLContext ? =>
    """
    Use a PEM file to specify certificate authorities. Clients must either set
    this or use set_ca_path. For servers, it is optional.
    """
    if @SSL_CTX_load_verify_locations[I32](_ctx, file.cstring(),
      Pointer[U8]) == 0
    then
      error
    end
    this

  fun ref set_ca_path(path: String): SSLContext ? =>
    """
    Use a directory to specify certificate authorities. Clients must either set
    this or use set_ca_file. For servers, it is optional.
    """
    if @SSL_CTX_load_verify_locations[I32](_ctx, Pointer[U8],
      path.cstring()) == 0
    then
      error
    end
    this

  fun ref set_ciphers(list: String): SSLContext ? =>
    """
    Set the accepted ciphers. This replaces the existing list.
    """
    if @SSL_CTX_set_cipher_list[I32](_ctx, list.cstring()) == 0 then error end
    this

  fun ref set_verify(state: Bool): SSLContext =>
    """
    Set to true to require verification. Defaults to true.
    """
    @SSL_CTX_set_verify[None](_ctx, if state then I32(3) else I32(0) end,
      Pointer[U8])
    this

  fun ref dispose() =>
    """
    Dispose of the context.
    """
    if not _ctx.is_null() then
      @SSL_CTX_free[None](_ctx)
      _ctx = Pointer[_SSLContext]
    end

  fun _context(): Pointer[_SSLContext] tag =>
    _ctx
