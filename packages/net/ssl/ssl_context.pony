use "files"

use @SSL_CTX_ctrl[ILong](
  ctx: Pointer[_SSLContext] tag,
  op: I32,
  arg: ULong,
  parg: Pointer[None])

use @SSLv23_method[Pointer[None]]()
use @TLS_method[Pointer[None]]()
use @SSL_CTX_new[Pointer[_SSLContext]](method: Pointer[None])
use @SSL_CTX_free[None](ctx: Pointer[_SSLContext] tag)
use @SSL_CTX_clear_options[ULong](ctx: Pointer[_SSLContext] tag, opts: ULong)
use @SSL_CTX_set_options[ULong](ctx: Pointer[_SSLContext] tag, opts: ULong)

primitive _SSLContext

primitive _SslCtrlSetOptions                fun val apply(): I32 => 32
primitive _SslCtrlClearOptions              fun val apply(): I32 => 77

// These are the SSL_OP_NO_{SSL|TLS}vx{_x} in ssl.h.
// Since Pony doesn't allow underscore we use camel case
// and began them with underscore to keep them private.
// Also, in the version strings the "v" becomes "V" and
// the underscore "_" becomes "u". So SSL_OP_NO_TLSv1_2
// _SslOpNo_TlsV1u2.
primitive _SslOpNoSslV2    fun val apply(): ULong =>    0x01000000 // 0 in 1.1
primitive _SslOpNoSslV3    fun val apply(): ULong =>    0x02000000
primitive _SslOpNoTlsV1    fun val apply(): ULong =>    0x04000000
primitive _SslOpNoTlsV1u2  fun val apply(): ULong =>    0x08000000
primitive _SslOpNoTlsV1u1  fun val apply(): ULong =>    0x10000000
primitive _SslOpNoTlsV1u3  fun val apply(): ULong =>    0x20000000

primitive _SslOpNoDtlsV1   fun val apply(): ULong =>    0x04000000
primitive _SslOpNoDtlsV1u2 fun val apply(): ULong =>    0x08000000

// Defined as SSL_OP_NO_SSL_MASK in ssl.h
primitive _SslOpNoSslMask fun val apply(): ULong =>
    (_SslOpNoSslV3.apply() + _SslOpNoTlsV1.apply() + _SslOpNoTlsV1u1.apply()
      + _SslOpNoTlsV1u2.apply() + _SslOpNoTlsV1u3.apply())

// Defined as SSL_OP_NO_DTLS_MASK in ssl.h
primitive _SslOpNoDtlsMask fun val apply(): ULong =>
    (_SslOpNoDtlsV1.apply() + _SslOpNoDtlsV1u2.apply())

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
    ifdef "openssl_1.1.x" then
      _ctx = @SSL_CTX_new(@TLS_method())

      // Allow only newer ciphers.
      try
        set_min_proto_version(Tls1u2Version.apply())?
        set_max_proto_version(SslAutoVersion.apply())?
      end
    else
      _ctx = @SSL_CTX_new(@SSLv23_method())

      // Disable "all" SSL/TSL options
      _set_options(_SslOpNoSslMask.apply() + _SslOpNoSslV2.apply())

      // Allow only newer ciphers
      allow_tls_v1_2(true)
    end

  fun _set_options(opts: ULong) =>
    ifdef "openssl_1.1.x" then
      @SSL_CTX_set_options(_ctx, opts)
    else
      @SSL_CTX_ctrl(_ctx, _SslCtrlSetOptions.apply(), opts, Pointer[None])
    end

  fun _clear_options(opts: ULong) =>
    ifdef "openssl_1.1.x" then
      @SSL_CTX_clear_options(_ctx, opts)
    else
      @SSL_CTX_ctrl(_ctx, _SslCtrlClearOptions.apply(), opts, Pointer[None])
    end

  fun client(hostname: String = ""): SSL iso^ ? =>
    """
    Create a client-side SSL session. If a hostname is supplied, the server
    side certificate must be valid for that hostname.
    """
    let ctx = _ctx
    let verify = _client_verify
    recover SSL._create(ctx, false, verify, hostname)? end

  fun server(): SSL iso^ ? =>
    """
    Create a server-side SSL session.
    """
    let ctx = _ctx
    let verify = _server_verify
    recover SSL._create(ctx, true, verify)? end

  fun ref set_cert(cert: FilePath, key: FilePath) ? =>
    """
    The cert file is a PEM certificate chain. The key file is a private key.
    Servers must set this. For clients, it is optional.
    """
    if
      _ctx.is_null()
        or (cert.path.size() == 0)
        or (key.path.size() == 0)
        or (0 == @SSL_CTX_use_certificate_chain_file[I32](
          _ctx, cert.path.cstring()))
        or (0 == @SSL_CTX_use_PrivateKey_file[I32](
          _ctx, key.path.cstring(), I32(1)))
        or (0 == @SSL_CTX_check_private_key[I32](_ctx))
    then
      error
    end

  fun ref set_authority(
    file: (FilePath | None),
    path: (FilePath | None) = None)
    ?
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
      _ctx.is_null()
        or (f.is_null() and p.is_null())
        or (0 == @SSL_CTX_load_verify_locations[I32](_ctx, f, p))
    then
      error
    end

  fun ref set_ciphers(ciphers: String) ? =>
    """
    Set the accepted ciphers. This replaces the existing list. Raises an error
    if the cipher list is invalid.
    """
    if
      _ctx.is_null()
        or (0 == @SSL_CTX_set_cipher_list[I32](_ctx, ciphers.cstring()))
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

  fun ref set_min_proto_version(version: ULong) ? =>
    """
    Set minimum protocol version. Set to SslAutoVersion, 0,
    to automatically manage lowest version.

    Supported versions: Ssl3Version, Tls1Version, Tls1u1Version,
                        Tls1u2Version, Tls1u3Version, Dtls1Version,
                        Dtls1u2Version
    """
    let result = @SSL_CTX_ctrl(_ctx, _SslCtrlSetMinProtoVersion.apply(),
        version, Pointer[None])
    if result == 0 then
      error
    end

  fun ref get_min_proto_version(): ILong =>
    """
    Get minimum protocol version. Returns SslAutoVersion, 0,
    when automatically managing lowest version.

    Supported versions: Ssl3Version, Tls1Version, Tls1u1Version,
                        Tls1u2Version, Tls1u3Version, Dtls1Version,
                        Dtls1u2Version
    """
    @SSL_CTX_ctrl(_ctx, _SslCtrlGetMinProtoVersion.apply(), 0, Pointer[None])

  fun ref set_max_proto_version(version: ULong) ? =>
    """
    Set maximum protocol version. Set to SslAutoVersion, 0,
    to automatically manage higest version.

    Supported versions: Ssl3Version, Tls1Version, Tls1u1Version,
                        Tls1u2Version, Tls1u3Version, Dtls1Version,
                        Dtls1u2Version
    """
    let result = @SSL_CTX_ctrl(_ctx, _SslCtrlSetMaxProtoVersion.apply(),
        version, Pointer[None])
    if result == 0 then
      error
    end

  fun ref get_max_proto_version(): ILong =>
    """
    Get maximum protocol version. Returns SslAutoVersion, 0,
    when automatically managing highest version.

    Supported versions: Ssl3Version, Tls1Version, Tls1u1Version,
                        Tls1u2Version, Tls1u3Version, Dtls1Version,
                        Dtls1u2Version
    """
    @SSL_CTX_ctrl(_ctx, _SslCtrlGetMaxProtoVersion.apply(), 0, Pointer[None])

  fun ref alpn_set_resolver(resolver: ALPNProtocolResolver box): Bool =>
    """
    Use `resolver` to choose the protocol to be selected for incomming connections.

    Returns true on success
    Requires OpenSSL >= 1.0.2
    """
    ifdef "openssl_1.1.x" then
      @SSL_CTX_set_alpn_select_cb[None](_ctx, addressof SSLContext._alpn_select_cb, resolver)
      return true
    end

    false
  
  fun ref alpn_set_client_protocols(protocols: Array[String] box): Bool =>
    """
    Configures the SSLContext to advertise the protocol names defined in `protocols` when connecting to a server
    protocol names must have a size of 1 to 255

    Returns true on success
    Requires OpenSSL >= 1.0.2
    """
    ifdef "openssl_1.1.x" then
      try
        let proto_list = _ALPNProtocolList.from_array(protocols)?
        let result = @SSL_CTX_set_alpn_protos[I32](_ctx, proto_list.cpointer(), proto_list.size())
        return result == 0
      end
    end

    false
  
  fun @_alpn_select_cb(
    ssl: Pointer[_SSL] tag,
    out: Pointer[Pointer[U8] tag] tag,
    outlen: Pointer[U8] tag,
    inptr: Pointer[U8] box,
    inlen: U32 val,
    resolver: ALPNProtocolResolver box)
    : I32 val
  =>
    let proto_arr_str = String.copy_cpointer(inptr, USize.from[U32](inlen))
    try
      let proto_arr = _ALPNProtocolList.to_array(proto_arr_str)?

      match resolver.resolve(proto_arr)
      | let matched: String val =>
      var size = matched.size()
      if (size > 0) and (size <= 255) then
        var ptr = matched.cpointer()
        @memcpy[None](out, addressof ptr, size.bitwidth() / 8)
        @memcpy[None](outlen, addressof size, USize(1))
        _ALPNMatchResultCode.ok()
      else
        _ALPNMatchResultCode.fatal()
      end
      | ALPNNoAck => _ALPNMatchResultCode.no_ack()
      | ALPNWarning => _ALPNMatchResultCode.warning()
      | ALPNFatal => _ALPNMatchResultCode.fatal()
      end
    else
      _ALPNMatchResultCode.fatal()
    end

  fun ref allow_tls_v1(state: Bool) =>
    """
    Allow TLS v1. Defaults to false.
    Deprecated: use set_min_proto_version and set_max_proto_version
    """
    if not _ctx.is_null() then
      if state then
        _clear_options(_SslOpNoTlsV1.apply())
      else
        _set_options(_SslOpNoTlsV1.apply())
      end
    end

  fun ref allow_tls_v1_1(state: Bool) =>
    """
    Allow TLS v1.1. Defaults to false.
    Deprecated: use set_min_proto_version and set_max_proto_version
    """
    if not _ctx.is_null() then
      if state then
        _clear_options(_SslOpNoTlsV1u1.apply())
      else
        _set_options(_SslOpNoTlsV1u1.apply())
      end
    end

  fun ref allow_tls_v1_2(state: Bool) =>
    """
    Allow TLS v1.2. Defaults to true.
    Deprecated: use set_min_proto_version and set_max_proto_version
    """
    if not _ctx.is_null() then
      if state then
        _clear_options(_SslOpNoTlsV1u2.apply())
      else
        _set_options(_SslOpNoTlsV1u2.apply())
      end
    end

  fun ref dispose() =>
    """
    Free the SSL context.
    """
    if not _ctx.is_null() then
      @SSL_CTX_free(_ctx)
      _ctx = Pointer[_SSLContext]
    end

  fun _final() =>
    """
    Free the SSL context.
    """
    if not _ctx.is_null() then
      @SSL_CTX_free(_ctx)
    end
