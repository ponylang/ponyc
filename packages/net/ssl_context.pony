use "files"

use "lib:crypt32" if windows
use "lib:cryptui" if windows
use "lib:bcrypt" if windows

use @memcpy[Pointer[None]](dst: Pointer[None] tag, src: Pointer[None] tag,
  n: USize)
use @SSL_CTX_ctrl[ILong](
  ctx: Pointer[_SSLContext] tag,
  op: I32,
  arg: ILong,
  parg: Pointer[None])
use @TLS_method[Pointer[_SSLMethod]]()
  if "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
use @SSL_CTX_new[Pointer[_SSLContext]](method: Pointer[_SSLMethod])
use @SSL_CTX_free[None](ctx: Pointer[_SSLContext] tag)
use @SSL_CTX_clear_options[U64](
  ctx: Pointer[_SSLContext] tag, opts: U64)
  if "openssl_3.0.x" or "openssl_4.0.x"
use @SSL_CTX_clear_options[ULong](
  ctx: Pointer[_SSLContext] tag, opts: ULong)
  if "openssl_1.1.x" and
    not ("openssl_3.0.x" or "openssl_4.0.x")
use @SSL_CTX_set_options[U64](
  ctx: Pointer[_SSLContext] tag, opts: U64)
  if "openssl_3.0.x" or "openssl_4.0.x"
use @SSL_CTX_set_options[ULong](
  ctx: Pointer[_SSLContext] tag, opts: ULong)
  if "openssl_1.1.x" and
    not ("openssl_3.0.x" or "openssl_4.0.x")
use @SSL_CTX_use_certificate_chain_file[I32](
  ctx: Pointer[_SSLContext] tag,
  file: Pointer[U8] tag)
use @SSL_CTX_use_PrivateKey_file[I32](
  ctx: Pointer[_SSLContext] tag,
  file: Pointer[U8] tag,
  typ: I32)
use @SSL_CTX_check_private_key[I32](ctx: Pointer[_SSLContext] tag)
use @SSL_CTX_load_verify_locations[I32](
  ctx: Pointer[_SSLContext] tag,
  ca_file: Pointer[U8] tag,
  ca_path: Pointer[U8] tag)
use @X509_STORE_new[Pointer[_X509Store] tag]()
use @CertOpenSystemStoreA[Pointer[_CertStore] tag](
  prov: Pointer[None],
  protocol: Pointer[U8] tag)
  if windows
use @CertEnumCertificatesInStore[NullablePointer[_CertContext]](
  cert_store: Pointer[_CertStore] tag,
  prev_ctx: NullablePointer[_CertContext])
  if windows
use @CertFreeCertificateContext[I32](cert_ctx: NullablePointer[_CertContext])
  if windows
use @d2i_X509[Pointer[X509] tag](
  val_out: Pointer[Pointer[X509]],
  der_in: Pointer[Pointer[U8]],
  length: ILong)
use @X509_STORE_add_cert[I32](store: Pointer[_X509Store] tag,
  x509: Pointer[X509] tag)
use @X509_free[None](x509: Pointer[X509] tag)
use @SSL_CTX_set_cert_store[None](
  ctx: Pointer[_SSLContext] tag,
  store: Pointer[_X509Store] tag)
use @X509_STORE_free[None](store: Pointer[_X509Store] tag)
use @CertCloseStore[I32](store: Pointer[_CertStore] tag, flags: U32)
  if windows
use @SSL_CTX_set_cipher_list[I32](
  ctx: Pointer[_SSLContext] tag,
  control: Pointer[U8] tag)
use @SSL_CTX_set_verify_depth[None](ctx: Pointer[_SSLContext] tag, depth: I32)
use @SSL_CTX_set_alpn_select_cb[None](
  ctx: Pointer[_SSLContext] tag,
  cb: _ALPNSelectCallback,
  resolver: ALPNProtocolResolver val)
  if "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
use @SSL_CTX_set_alpn_protos[I32](
  ctx: Pointer[_SSLContext] tag,
  protos: Pointer[U8] tag,
  protos_len: U32)
  if "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"

primitive _SSLMethod
primitive _SSLContext
primitive _X509Store
primitive _CertStore

primitive _SSLCtrlSetOptions
  fun val apply(): I32 => 32

primitive _SSLCtrlClearOptions
  fun val apply(): I32 => 77

// The SSL_OP_NO_{SSL,TLS}vX[_Y] options from ssl.h. A Pony type name takes no
// underscores, so the `v` becomes `V` and the `_` becomes `u`:
// SSL_OP_NO_TLSv1_2 is _SSLOpNoTLSV1u2.
primitive _SSLOpNoTLSV1
  fun val apply(): U64 => 0x04000000

primitive _SSLOpNoTLSV1u2
  fun val apply(): U64 => 0x08000000

primitive _SSLOpNoTLSV1u1
  fun val apply(): U64 => 0x10000000

primitive _SSLOpNoTLSV1u3
  fun val apply(): U64 => 0x20000000

class val SSLContext
  """
  An SSL context is used to create SSL sessions.
  """
  var _ctx: Pointer[_SSLContext] tag
  var _client_verify: Bool = true
  var _server_verify: Bool = false
  // OpenSSL holds the raw pointer to this, and the Pony garbage collector
  // cannot see that. The field is what keeps the resolver alive.
  var _alpn_resolver: (ALPNProtocolResolver val | None) = None

  new create() =>
    """
    Create an SSL context.
    """
    ifdef
      "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
    then
      _ctx = @SSL_CTX_new(@TLS_method())

      // Allow only newer ciphers. These raise only if `SSL_CTX_new` failed,
      // and a null context can never make a session, so the swallow is safe.
      try
        set_min_proto_version(TLS1u2Version())?
        set_max_proto_version(SSLAutoVersion())?
      end
    else
      compile_error "You must select an SSL version to use."
    end

  fun ref _set_options(opts: U64) =>
    ifdef "openssl_3.0.x" or "openssl_4.0.x" then
      @SSL_CTX_set_options(_ctx, opts)
    elseif "openssl_1.1.x" then
      @SSL_CTX_set_options(_ctx, opts.ulong())
    elseif "libressl" then
      @SSL_CTX_ctrl(_ctx, _SSLCtrlSetOptions(), opts.ilong(), Pointer[None])
    else
      compile_error "You must select an SSL version to use."
    end

  fun ref _clear_options(opts: U64) =>
    ifdef "openssl_3.0.x" or "openssl_4.0.x" then
      @SSL_CTX_clear_options(_ctx, opts)
    elseif "openssl_1.1.x" then
      @SSL_CTX_clear_options(_ctx, opts.ulong())
    elseif "libressl" then
      @SSL_CTX_ctrl(_ctx, _SSLCtrlClearOptions(), opts.ilong(), Pointer[None])
    else
      compile_error "You must select an SSL version to use."
    end

  fun _ssl_ctx(): Pointer[_SSLContext] tag =>
    """
    The raw `SSL_CTX` handle, for a session being created from this context.
    """
    _ctx

  fun val client(hostname: String = ""): SSL iso^ ? =>
    """
    Create a client-side SSL session. If a hostname is supplied and client
    verification is on, the server side certificate must be valid for that
    hostname. Raises an error if the context has been disposed.

    The session holds the context, so the context lives for as long as the
    session can handshake.
    """
    let verify = _client_verify
    recover SSL._create(this, false, verify, hostname)? end

  fun val server(): SSL iso^ ? =>
    """
    Create a server-side SSL session. Raises an error if the context has been
    disposed.

    The session holds the context, so the context and the ALPN resolver it
    installed with OpenSSL live for as long as the session can handshake.
    """
    let verify = _server_verify
    recover SSL._create(this, true, verify)? end

  fun ref set_cert(cert: FilePath, key: FilePath) ? =>
    """
    The cert file is a PEM certificate chain. The key file is a private key.
    Servers must set this. For clients, it is optional. Raises an error if the
    context has been disposed.
    """
    if _ctx.is_null() then error end

    if
      (cert.path.size() == 0) or
        (key.path.size() == 0) or
        (0 == @SSL_CTX_use_certificate_chain_file(
          _ctx, cert.path.cstring())) or
        (0 == @SSL_CTX_use_PrivateKey_file(
          _ctx, key.path.cstring(), I32(1))) or
        (0 == @SSL_CTX_check_private_key(_ctx))
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
    aren't valid, or if the context has been disposed.

    If both `file` and `path` are `None`, on Windows this method loads the
    system root certificates. On Posix it raises an error.
    """
    if _ctx.is_null() then error end

    if (file is None) and (path is None) then
      ifdef windows then
        _load_windows_root_certs()?
      else
        error
      end
    else
      let fs = try (file as FilePath).path else "" end
      let ps = try (path as FilePath).path else "" end

      let f = if fs.size() > 0 then fs.cstring() else Pointer[U8] end
      let p = if ps.size() > 0 then ps.cstring() else Pointer[U8] end

      if
        (f.is_null() and p.is_null()) or
          (0 == @SSL_CTX_load_verify_locations(_ctx, f, p))
      then
        error
      end
    end

  fun ref _load_windows_root_certs() ? =>
    ifdef windows then
      let root_str = "ROOT"
      let h_store = @CertOpenSystemStoreA(Pointer[None], root_str.cstring())
      if h_store.is_null() then error end

      let x509_store = @X509_STORE_new()
      if x509_store.is_null() then
        // The `try` below has not started, so its `then` clause will not close
        // the store.
        @CertCloseStore(h_store, U32(0))
        error
      end

      var p_context =
        @CertEnumCertificatesInStore(
          h_store, NullablePointer[_CertContext].none())

      try
        while true do
          // `CertEnumCertificatesInStore` returns null when there is no next
          // certificate, so a null `p_context` is the end of the enumeration.
          let cert_context = try p_context()? else break end
          let x509 =
            @d2i_X509(
              Pointer[Pointer[X509]],
              addressof cert_context.pb_cert_encoded,
              cert_context.cb_cert_encoded.ilong())
          if not x509.is_null() then
            let result = @X509_STORE_add_cert(x509_store, x509)
            @X509_free(x509)
            if result != 1 then error end
          end

          p_context = @CertEnumCertificatesInStore(h_store, p_context)
        end

        @SSL_CTX_set_cert_store(_ctx, x509_store)
      else
        // `CertEnumCertificatesInStore` frees the context handed to it as
        // `prev_ctx`, and returns none when the enumeration is done. Leaving
        // the loop any other way leaves the last context it returned for the
        // caller to free.
        if not p_context.is_none() then
          @CertFreeCertificateContext(p_context)
        end
        @X509_STORE_free(x509_store)
      then
        @CertCloseStore(h_store, U32(0))
      end
    end

  fun ref set_ciphers(ciphers: String) ? =>
    """
    Set the accepted ciphers. This replaces the existing list. Raises an error
    if the cipher list is invalid, or if the context has been disposed.
    """
    if _ctx.is_null() then error end

    if 0 == @SSL_CTX_set_cipher_list(_ctx, ciphers.cstring()) then
      error
    end

  fun ref set_client_verify(state: Bool) =>
    """
    Set to true to require verification. Defaults to true.

    A client session created with `state` false never reports `SSLAuthFail`,
    and an `SSLConnection` wrapping one never calls the wrapped protocol's
    `auth_failed`.
    """
    _client_verify = state

  fun ref set_server_verify(state: Bool) =>
    """
    Set to true to require verification. Defaults to false.

    A server session created with `state` false never reports `SSLAuthFail`,
    and an `SSLConnection` wrapping one never calls the wrapped protocol's
    `auth_failed`. It sends no certificate request, so it has no peer identity
    to reject.
    """
    _server_verify = state

  fun ref set_verify_depth(depth: U32) =>
    """
    Set the verify depth. Defaults to 6. Does nothing if the context has been
    disposed.

    A depth of 2^31 or more arrives at the SSL library as a negative depth.
    What each backend does with one is undocumented, so do not use a depth that
    large.
    """
    if not _ctx.is_null() then
      @SSL_CTX_set_verify_depth(_ctx, depth.i32())
    end

  fun ref set_min_proto_version(version: ULong) ? =>
    """
    Set minimum protocol version. Set to SSLAutoVersion, 0, to automatically
    manage lowest version.

    Raises an error if the context has been disposed, if the version is above
    the current maximum, or if the SSL library rejects the version.

    Supported versions: SSL3Version, TLS1Version, TLS1u1Version,
                        TLS1u2Version, TLS1u3Version
    """
    if _ctx.is_null() then error end

    let new_min = version.ilong()
    let max_v = get_max_proto_version()
    if (new_min != 0) and (max_v != 0) and (new_min > max_v) then
      error
    end

    let result =
      @SSL_CTX_ctrl(
        _ctx, _SSLCtrlSetMinProtoVersion(), new_min, Pointer[None])
    if result == 0 then
      error
    end

  fun get_min_proto_version(): ILong =>
    """
    Get minimum protocol version. Returns SSLAutoVersion, 0,
    when automatically managing lowest version. A disposed context returns
    SSLAutoVersion.

    Supported versions: SSL3Version, TLS1Version, TLS1u1Version,
                        TLS1u2Version, TLS1u3Version
    """
    if _ctx.is_null() then return SSLAutoVersion().ilong() end

    @SSL_CTX_ctrl(_ctx, _SSLCtrlGetMinProtoVersion(), 0, Pointer[None])

  fun ref set_max_proto_version(version: ULong) ? =>
    """
    Set maximum protocol version. Set to SSLAutoVersion, 0, to automatically
    manage highest version.

    Raises an error if the context has been disposed, if the version is below
    the current minimum, or if the SSL library rejects the version.

    Supported versions: SSL3Version, TLS1Version, TLS1u1Version,
                        TLS1u2Version, TLS1u3Version
    """
    if _ctx.is_null() then error end

    let new_max = version.ilong()
    let min_v = get_min_proto_version()
    if (new_max != 0) and (min_v != 0) and (new_max < min_v) then
      error
    end

    let result =
      @SSL_CTX_ctrl(
        _ctx, _SSLCtrlSetMaxProtoVersion(), new_max, Pointer[None])
    if result == 0 then
      error
    end

  fun get_max_proto_version(): ILong =>
    """
    Get maximum protocol version. Returns SSLAutoVersion, 0,
    when automatically managing highest version. A disposed context returns
    SSLAutoVersion.

    Supported versions: SSL3Version, TLS1Version, TLS1u1Version,
                        TLS1u2Version, TLS1u3Version
    """
    if _ctx.is_null() then return SSLAutoVersion().ilong() end

    @SSL_CTX_ctrl(_ctx, _SSLCtrlGetMaxProtoVersion(), 0, Pointer[None])

  fun ref alpn_set_resolver(resolver: ALPNProtocolResolver val): Bool =>
    """
    Use `resolver` to choose the protocol to be selected for incoming
    connections.

    OpenSSL holds a raw pointer to `resolver` that the Pony garbage collector
    cannot see. The context keeps `resolver` alive, and every session made from
    the context keeps the context alive, so `resolver` lives for as long as any
    session that can reach it. The resolver has to be set before any session is
    created, which the capabilities enforce: this method needs a mutable
    context, and `client` and `server` need one that has been made immutable.

    Returns true on success. Returns false if the context has been disposed.
    """
    if _ctx.is_null() then return false end

    ifdef
      "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
    then
      _alpn_resolver = resolver
      @SSL_CTX_set_alpn_select_cb(
        _ctx, addressof SSLContext._alpn_select_cb, resolver)
      return true
    else
      compile_error "You must select an SSL version to use."
    end

  fun ref alpn_set_client_protocols(protocols: Array[String] box): Bool =>
    """
    Advertise the protocol names in `protocols` when connecting to a server.
    Each name must be between 1 and 255 bytes.

    Returns true on success. Returns false if the context has been disposed,
    if `protocols` is empty or holds a name of an unusable size, or if OpenSSL
    would not take the list.
    """
    if _ctx.is_null() then return false end

    ifdef
      "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
    then
      try
        let proto_list = _ALPNProtocolList.from_array(protocols)?
        let result =
          @SSL_CTX_set_alpn_protos(
            _ctx, proto_list.cpointer(), proto_list.size().u32())
        return result == 0
      end
    else
      compile_error "You must select an SSL version to use."
    end

    false

  fun @_alpn_select_cb(
    ssl: Pointer[_SSL] tag,
    out: Pointer[Pointer[U8] tag] tag,
    outlen: Pointer[U8] tag,
    inptr: Pointer[U8] box,
    inlen: U32,
    resolver: ALPNProtocolResolver val)
    : I32
  =>
    let proto_arr_str = String.copy_cpointer(inptr, USize.from[U32](inlen))
    try
      let proto_arr = _ALPNProtocolList.to_array(proto_arr_str)?

      match \exhaustive\ resolver.resolve(proto_arr)
      | let matched: String =>
        let size = matched.size()
        if (size > 0) and (size <= 255) then
          // OpenSSL reads the pointer written to `out` after this call returns,
          // and only `inptr` and buffers that outlive the handshake are still
          // valid then. A name the resolver built here is neither, so point
          // into `inptr`. `proto_arr_str` is a byte for byte copy of it, so an
          // offset into one is an offset into the other. A name that is nowhere
          // in `inptr` is one the client never advertised, and raises.
          let offset = _ALPNProtocolList.offset_of(proto_arr_str, matched)?
          var ptr = inptr.offset(offset)
          var len = size.u8()
          @memcpy(out, addressof ptr, USize(0).bytewidth())
          @memcpy(outlen, addressof len, USize(1))
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
    Allow TLS v1. Defaults to false. Does nothing if the context has been
    disposed.
    Deprecated: use set_min_proto_version and set_max_proto_version
    """
    if not _ctx.is_null() then
      if state then
        _clear_options(_SSLOpNoTLSV1())
      else
        _set_options(_SSLOpNoTLSV1())
      end
    end

  fun ref allow_tls_v1_1(state: Bool) =>
    """
    Allow TLS v1.1. Defaults to false. Does nothing if the context has been
    disposed.
    Deprecated: use set_min_proto_version and set_max_proto_version
    """
    if not _ctx.is_null() then
      if state then
        _clear_options(_SSLOpNoTLSV1u1())
      else
        _set_options(_SSLOpNoTLSV1u1())
      end
    end

  fun ref allow_tls_v1_2(state: Bool) =>
    """
    Allow TLS v1.2. Defaults to true. Does nothing if the context has been
    disposed.
    Deprecated: use set_min_proto_version and set_max_proto_version
    """
    if not _ctx.is_null() then
      if state then
        _clear_options(_SSLOpNoTLSV1u2())
      else
        _set_options(_SSLOpNoTLSV1u2())
      end
    end

  fun ref dispose() =>
    """
    Free the SSL context. A disposed context cannot create a session, and no
    configuration of it can take effect. Every method that hands the context to
    OpenSSL says in its own docstring what it does once the context has been
    disposed.
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

struct _CertContext
  """
  The leading fields of Windows' `CERT_CONTEXT`, whose remaining fields nothing
  here reads. `CertEnumCertificatesInStore` returns a pointer to one of these,
  so the fields have to be declared in the order the C struct declares them.
  """
  var dw_cert_encoding_type: U32 = 0
  var pb_cert_encoded: Pointer[U8] = Pointer[U8]
  var cb_cert_encoded: U32 = 0
