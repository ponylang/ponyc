use @SSL_CTX_ctrl[I32](ctx: Pointer[_SSLContext], op: I32, arg: I32,
  parg: Pointer[U8] tag) if windows

use @SSL_CTX_ctrl[I64](ctx: Pointer[_SSLContext], op: I32, arg: I64,
  parg: Pointer[U8] tag) if not windows

primitive _SSLContext

class SSLContext val
  """
  An SSL context is used to create SSL sessions.
  """
  var _cert: String = ""
  var _key: String = ""
  var _ca_file: String = ""
  var _ca_path: String = ""
  var _ciphers: String = "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"
  var _client_verify: Bool = true
  var _server_verify: Bool = false
  var _verify_depth: U32 = 6
  var _disable_tls_v1: Bool = false
  var _disable_tls_v1_1: Bool = false
  var _disable_tls_v1_2: Bool = false

  fun client(hostname: String = ""): SSL iso^ ? =>
    """
    Create a client-side SSL session. If a hostname is supplied, the server
    side certificate must be valid for that hostname.
    """
    let ctx = _context(false)
    recover SSL._create(ctx, false, hostname) end

  fun server(): SSL iso^ ? =>
    """
    Create a server-side SSL session.
    """
    let ctx = _context(true)
    recover SSL._create(ctx, true) end

  fun ref set_cert(cert: String, key: String): SSLContext ref^ =>
    """
    The cert file is a PEM certificate chain. The key file is a private key.
    Servers must set this. For clients, it is optional.
    """
    _cert = cert
    _key = key
    this

  fun ref set_ca_file(ca_file: String): SSLContext ref^ =>
    """
    Use a PEM file to specify certificate authorities. Clients must either set
    this or use set_ca_path. For servers, it is optional.
    """
    _ca_file = ca_file
    this

  fun ref set_ca_path(ca_path: String): SSLContext ref^ =>
    """
    Use a directory to specify certificate authorities. Clients must either set
    this or use set_ca_file. For servers, it is optional.
    """
    _ca_path = ca_path
    this

  fun ref set_ciphers(ciphers: String): SSLContext ref^ =>
    """
    Set the accepted ciphers. This replaces the existing list.
    """
    _ciphers = ciphers
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
    _verify_depth = depth
    this

  fun ref set_disable_tls_v1(state: Bool): SSLContext ref^ =>
    """
    Disable TLS v1. Defaults to false.
    """
    _disable_tls_v1 = state
    this

  fun ref set_disable_tls_v1_1(state: Bool): SSLContext ref^ =>
    """
    Disable TLS v1.1. Defaults to false.
    """
    _disable_tls_v1_1 = state
    this

  fun ref set_disable_tls_v1_2(state: Bool): SSLContext ref^ =>
    """
    Disable TLS v1.2. Defaults to false.
    """
    _disable_tls_v1_2 = state
    this

  fun _context(server_side: Bool): Pointer[_SSLContext] tag ? =>
    """
    Initialise an SSL context.
    """
    var ctx = @SSL_CTX_new[Pointer[_SSLContext]](@SSLv23_method[Pointer[U8]]())

    if
      (_cert.size() > 0) and
      (_key.size() > 0) and
      ((@SSL_CTX_use_certificate_chain_file[I32](ctx, _cert.cstring()) == 0) or
      (@SSL_CTX_use_PrivateKey_file[I32](ctx, _key.cstring(), I32(1)) == 0) or
      (@SSL_CTX_check_private_key[I32](ctx) == 0))
    then
      error
    end

    if
      (server_side and _server_verify) or (not server_side and _client_verify)
    then
      let file = if _ca_file.size() > 0 then _ca_file.cstring()
        else Pointer[U8] end
      let path = if _ca_path.size() > 0 then _ca_path.cstring()
        else Pointer[U8] end

      if
        (file.is_null() and path.is_null()) or
        (@SSL_CTX_load_verify_locations[I32](ctx, file, path) == 0)
      then
        error
      end

      @SSL_CTX_set_verify[None](ctx, I32(3), Pointer[U8])
    else
      @SSL_CTX_set_verify[None](ctx, I32(0), Pointer[U8])
    end

    @SSL_CTX_set_verify_depth[None](ctx, _verify_depth)

    // set SSL_OP_NO_SSLv2
    @SSL_CTX_ctrl(ctx, 32, 0x01000000, Pointer[U8])

    // set SSL_OP_NO_SSLv3
    @SSL_CTX_ctrl(ctx, 32, 0x02000000, Pointer[U8])

    // set SSL_OP_NO_TLSv1
    if _disable_tls_v1 then
      @SSL_CTX_ctrl(ctx, 32, 0x04000000, Pointer[U8])
    end

    // set SSL_OP_NO_TLSv1_1
    if _disable_tls_v1_1 then
      @SSL_CTX_ctrl(ctx, 32, 0x10000000, Pointer[U8])
    end

    // set SSL_OP_NO_TLSv1_2
    if _disable_tls_v1_2 then
      @SSL_CTX_ctrl(ctx, 32, 0x08000000, Pointer[U8])
    end

    if @SSL_CTX_set_cipher_list[I32](ctx, _ciphers.cstring()) == 0 then
      error
    end

    ctx
