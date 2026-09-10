use "files"

use @DTLS_method[Pointer[_SSLMethod]]()
  if "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"

class val DTLSContext
  """
  A DTLS context is used to create DTLS sessions.

  DTLS is the datagram variant of TLS. It provides the same authentication and
  encryption guarantees over unreliable transports like UDP. A DTLS context uses
  `DTLS_method()` internally and accepts only DTLS version numbers.

  This type is separate from `SSLContext` because DTLS and TLS are not
  interchangeable: a TLS context cannot create a DTLS session and vice versa.
  Keeping them separate makes the wrong combination a compile error.
  """
  var _ctx: Pointer[_SSLContext] tag
  var _client_verify: Bool = true
  var _server_verify: Bool = false
  var _alpn_resolver: (ALPNProtocolResolver val | None) = None

  new create() =>
    """
    Create a DTLS context.
    """
    ifdef
      "openssl_1.1.x" or "openssl_3.0.x" or "openssl_4.0.x" or "libressl"
    then
      _ctx = @SSL_CTX_new(@DTLS_method())

      try
        set_min_proto_version(DTLS1u2Version())?
        set_max_proto_version(SSLAutoVersion())?
      end
    else
      compile_error "You must select an SSL version to use."
    end

  fun _ssl_ctx(): Pointer[_SSLContext] tag =>
    _ctx

  fun val client(hostname: String = ""): DTLS iso^ ? =>
    """
    Create a client-side DTLS session. If a hostname is supplied and client
    verification is on, the server side certificate must be valid for that
    hostname. Raises an error if the context has been disposed.

    The session holds the context, so the context lives for as long as the
    session can handshake.
    """
    let verify = _client_verify
    recover DTLS._create(this, false, verify, hostname)? end

  fun val server(): DTLS iso^ ? =>
    """
    Create a server-side DTLS session. Raises an error if the context has been
    disposed.

    The session holds the context, so the context and the ALPN resolver it
    installed with OpenSSL live for as long as the session can handshake.
    """
    let verify = _server_verify
    recover DTLS._create(this, true, verify)? end

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
        @CertCloseStore(h_store, U32(0))
        error
      end

      var p_context =
        @CertEnumCertificatesInStore(
          h_store, NullablePointer[_CertContext].none())

      try
        while true do
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

    A client session created with `state` false never reports `SSLAuthFail`.
    """
    _client_verify = state

  fun ref set_server_verify(state: Bool) =>
    """
    Set to true to require verification. Defaults to false.

    A server session created with `state` false never reports `SSLAuthFail`.
    It sends no certificate request, so it has no peer identity to reject.
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

    Raises an error if the context has been disposed or if the SSL library
    rejects the version.

    Supported versions: DTLS1Version, DTLS1u2Version
    """
    if _ctx.is_null() then error end

    let result =
      @SSL_CTX_ctrl(
        _ctx, _SSLCtrlSetMinProtoVersion(), version.ilong(), Pointer[None])
    if result == 0 then
      error
    end

  fun get_min_proto_version(): ILong =>
    """
    Get minimum protocol version. Returns SSLAutoVersion, 0,
    when automatically managing lowest version. A disposed context returns
    SSLAutoVersion.

    Supported versions: DTLS1Version, DTLS1u2Version
    """
    if _ctx.is_null() then return SSLAutoVersion().ilong() end

    @SSL_CTX_ctrl(_ctx, _SSLCtrlGetMinProtoVersion(), 0, Pointer[None])

  fun ref set_max_proto_version(version: ULong) ? =>
    """
    Set maximum protocol version. Set to SSLAutoVersion, 0, to automatically
    manage highest version.

    Raises an error if the context has been disposed or if the SSL library
    rejects the version.

    Supported versions: DTLS1Version, DTLS1u2Version
    """
    if _ctx.is_null() then error end

    let result =
      @SSL_CTX_ctrl(
        _ctx, _SSLCtrlSetMaxProtoVersion(), version.ilong(), Pointer[None])
    if result == 0 then
      error
    end

  fun get_max_proto_version(): ILong =>
    """
    Get maximum protocol version. Returns SSLAutoVersion, 0,
    when automatically managing highest version. A disposed context returns
    SSLAutoVersion.

    Supported versions: DTLS1Version, DTLS1u2Version
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

  fun ref dispose() =>
    """
    Free the DTLS context. A disposed context cannot create a session, and no
    configuration of it can take effect.
    """
    if not _ctx.is_null() then
      @SSL_CTX_free(_ctx)
      _ctx = Pointer[_SSLContext]
    end

  fun _final() =>
    if not _ctx.is_null() then
      @SSL_CTX_free(_ctx)
    end
