use "pony_test"
use "pony_check"
use "files"
use "itertools"

use @memset[Pointer[None]](dst: Pointer[None], value: I32, n: USize)
use @pony_ctx[Pointer[None]]()
use @pony_triggergc[None](ctx: Pointer[None])
use @ERR_peek_error[ULong]()

class \nodoc\ iso _TestALPNProtocolListEncoding is UnitTest
  """
  `from_array` packs protocol names into a protocol name list, and raises on a
  name whose size is outside 1..255.
  """
  fun name(): String => "net/ssl/_ALPNProtocolList.from_array"

  fun apply(h: TestHelper) =>
    let valid_h2http11 = "\x02h2\x08http/1.1"

    h.assert_error(
      {()? => _ALPNProtocolList.from_array([""])? },
      "raise error on empty protocol identifier")
    h.assert_error(
      {()? => _ALPNProtocolList.from_array(["dummy"; ""])? },
      "raise error when encoding an protocol identifier")
    h.assert_error(
      {()? => _ALPNProtocolList.from_array([])? },
      "raise error when encoding an empty array")

    let id256chars =
      recover val String(256) .> concat(Iter[U8].repeat_value('A'), 0, 256) end
    h.assert_eq[USize](id256chars.size(), USize(256))
    h.assert_error(
      {()? => _ALPNProtocolList.from_array([id256chars])? },
      "raise error on identifier longer than 256 bytes.")
    h.assert_error(
      {()? => _ALPNProtocolList.from_array([id256chars; "dummy"])? },
      "raise error on identifier longer than 256 bytes.")

    try
      h.assert_eq[String](
        _ALPNProtocolList.from_array(["h2"; "http/1.1"])?, valid_h2http11)
    else
      h.fail("failed to encode an array of valid identifiers")
    end

class \nodoc\ iso _TestALPNProtocolListOffsetOf is UnitTest
  """
  `offset_of` finds a protocol name in a protocol list, and raises when the list
  does not contain it.

  The offset it returns is what the ALPN select callback hands to OpenSSL, so a
  wrong offset points OpenSSL at the wrong bytes.
  """
  fun name(): String => "net/ssl/_ALPNProtocolList.offset_of"

  fun apply(h: TestHelper) =>
    let h2_http11 = "\x02h2\x08http/1.1"

    try
      h.assert_eq[USize](
        1,
        _ALPNProtocolList.offset_of(h2_http11, "h2")?,
        "h2 starts one byte past its length prefix")
      h.assert_eq[USize](
        4,
        _ALPNProtocolList.offset_of(h2_http11, "http/1.1")?,
        "http/1.1 starts after h2 and its own length prefix")
    else
      h.fail("failed to find a name that is in the list")
    end

    h.assert_error(
      {()? => _ALPNProtocolList.offset_of(h2_http11, "spdy/1")? },
      "raise on a name the list does not contain")

    // A length prefix is what separates names, so a name that appears in the
    // list only as part of a longer name is not in the list.
    h.assert_error(
      {()? => _ALPNProtocolList.offset_of("\x03h2c", "h2")? },
      "h2 is not a prefix match inside h2c")
    h.assert_error(
      {()? => _ALPNProtocolList.offset_of("\x03xh2", "h2")? },
      "h2 is not a suffix match inside xh2")

    // "h2" appears inside "h2c" before it appears as a name of its own. The
    // offset has to be the name's, not the first place the bytes turn up.
    try
      h.assert_eq[USize](
        5,
        _ALPNProtocolList.offset_of("\x03h2c\x02h2", "h2")?,
        "a name that a longer name contains is found at its own offset")
    else
      h.fail("failed to find h2 past a longer name containing it")
    end

    try
      h.assert_eq[USize](
        1,
        _ALPNProtocolList.offset_of("\x02h2\x02h2", "h2")?,
        "a repeated name is found at the first of its offsets")
    else
      h.fail("failed to find a repeated name")
    end

    h.assert_error(
      {()? => _ALPNProtocolList.offset_of("", "h2")? },
      "raise on an empty list")
    h.assert_error(
      {()? => _ALPNProtocolList.offset_of("\x08http", "http")? },
      "raise on a list whose length prefix runs past its end")
    h.assert_error(
      {()? => _ALPNProtocolList.offset_of("\x00", "")? },
      "raise on a zero length prefix")

class \nodoc\ iso _TestALPNProtocolListOffsetOfRoundtrip
  is Property1[Array[String]]
  """
  Every name `from_array` packs into a list is found by `offset_of`, at an
  offset whose bytes are that name.
  """
  fun name(): String => "net/ssl/_ALPNProtocolList.offset_of/property/roundtrip"

  fun gen(): Generator[Array[String]] =>
    Generators.array_of[String](
      Generators.ascii_printable(1, 20) where from = 1, to = 5)

  fun ref property(sample: Array[String], h: PropertyHelper) ? =>
    let list = _ALPNProtocolList.from_array(sample)?

    for protocol in sample.values() do
      let offset = _ALPNProtocolList.offset_of(list, protocol)?

      // The byte before a name is its length. Checking it says the offset is
      // the start of a name and not somewhere in the middle of one, which is
      // the only way `offset_of` can be wrong while still matching the bytes.
      h.assert_eq[USize](
        protocol.size(),
        USize.from[U8](list(offset - 1)?),
        "the byte before the offset should be the name's length")
      h.assert_true(
        list.at(protocol, offset.isize()),
        "the bytes at the offset should be the protocol name")
    end

class \nodoc\ iso _TestALPNProtocolListDecode is UnitTest
  fun name(): String => "net/ssl/_ALPNProtocolList.to_array"

  fun apply(h: TestHelper) =>
    let valid_h2http11 = "\x02h2\x08http/1.1"
    try
      let decoded = _ALPNProtocolList.to_array(valid_h2http11)?
      h.assert_eq[USize](decoded.size(), USize(2))
      h.assert_eq[ALPNProtocolName](decoded(0)?, "h2")
      h.assert_eq[ALPNProtocolName](decoded(1)?, "http/1.1")
    else
      h.fail("failed to decode a valid protocol list")
    end

    h.assert_error(
      {()? => _ALPNProtocolList.to_array("")? },
      "raise error when decoding an empty protocol list")
    h.assert_error(
      {()? => _ALPNProtocolList.to_array("\x03h2")? },
      "raise error on malformed data")
    h.assert_error(
      {()? => _ALPNProtocolList.to_array("\x00")? },
      "raise error on malformed data")
    h.assert_error(
      {()? => _ALPNProtocolList.to_array("\x01A\x00")? },
      "raise error on malformed data")
    h.assert_error(
      {()? => _ALPNProtocolList.to_array("\x01A\x01")? },
      "raise error on malformed data")

class \nodoc\ iso _TestALPNStandardProtocolResolver is UnitTest
  fun name(): String => "net/ssl/StandardALPNProtocolResolver"

  fun apply(h: TestHelper) =>
    fallback_case(h)
    failure_case(h)
    match_cases(h)

  fun fallback_case(h: TestHelper) =>
    let resolver = ALPNStandardProtocolResolver(["h2"])

    match resolver.resolve(["http/1.1"])
    | "http/1.1" => None
    else
      h.fail(
        "ALPNStandardProtocolResolver didn't fall back to clients " +
        "first identifier, when it should have")
    end

  fun failure_case(h: TestHelper) =>
    let resolver = ALPNStandardProtocolResolver(["h2"], false)

    match resolver.resolve(["http/1.1"])
    | ALPNWarning => None
    else
      h.fail(
        "ALPNStandardProtocolResolver didn't return ALPNFailure, " +
        "when it should have")
    end

  fun match_cases(h: TestHelper) =>
    let resolver = ALPNStandardProtocolResolver(["h2"])

    match resolver.resolve(["dummy"; "h2"; "http/1.1"])
    | "h2" => None
    else
      h.fail("ALPNStandardProtocolResolver didn't return a matching protocol")
    end

primitive \nodoc\ _TestSSLDefaultSessions
  fun val apply(h: TestHelper): (SSL iso^, SSL iso^) ? =>
    """
    A client and a server session from a context that trusts the test
    certificate, presents it, and verifies neither side.
    """
    let sslctx =
      _TestSSLContext(
        h
        where cert = true,
          authority = true,
          client_verify = false,
          server_verify = false)?

    let ssl_client =
      try
        sslctx.client()?
      else
        h.fail("failed getting ssl client session")
        error
      end
    let ssl_server =
      try
        sslctx.server()?
      else
        h.fail("failed getting ssl server session")
        error
      end

    (consume ssl_client, consume ssl_server)

primitive \nodoc\ _TestSSLTransfer
  fun val apply(sender: SSL, receiver: SSL): SSLReceiveResult =>
    """
    Hand every encrypted byte the sender has ready to the receiver. Returns
    `SSLAccepted` when the sender had nothing to send.
    """
    var result: SSLReceiveResult = SSLAccepted
    while true do
      match \exhaustive\ sender.send()
      | let data: Array[U8] iso =>
        result = receiver.receive(consume data)
      | None => break
      end
    end
    result

primitive \nodoc\ _TestSSLCorruptRecord
  fun val apply(sender: SSL, receiver: SSL, payload: ByteSeq) ? =>
    """
    Write `payload` on the sender and hand the receiver the resulting
    ciphertext with its last byte flipped, which breaks the authentication tag
    of the record that byte belongs to.
    """
    sender.write(payload)?
    var record =
      match \exhaustive\ sender.send()
      | let data: Array[U8] iso => consume data
      | None => error
      end
    let last = record.size() - 1
    record(last)? = record(last)? xor 0xFF
    receiver.receive(consume record)
    // `send` returns one buffer, and a payload large enough to span records
    // leaves the rest queued. Hand those over too, so the receiver gets
    // everything the sender wrote.
    _TestSSLTransfer(sender, receiver)

primitive \nodoc\ _TestSSLContext
  fun val apply(
    h: TestHelper,
    cert: Bool = false,
    authority: Bool = false,
    client_verify: Bool = true,
    server_verify: Bool = false,
    min_proto: (ULong | None) = None,
    max_proto: (ULong | None) = None)
    : SSLContext val ?
  =>
    """
    A context with no certificate and no authority until asked for them. Every
    setting is a parameter, so a test that turns on what it is testing shows it
    at the call site.
    """
    let auth = FileAuth(h.env.root)

    try
      recover val
        let ctx: SSLContext ref = SSLContext
        if cert then
          ctx.set_cert(
            FilePath(auth, "assets/cert.pem"),
            FilePath(auth, "assets/key.pem"))?
        end
        if authority then
          ctx.set_authority(FilePath(auth, "assets/cert.pem"))?
        end
        ctx.set_client_verify(client_verify)
        ctx.set_server_verify(server_verify)
        match min_proto
        | let v: ULong => ctx.set_min_proto_version(v)?
        end
        match max_proto
        | let v: ULong => ctx.set_max_proto_version(v)?
        end
        ctx
      end
    else
      h.fail("ssl context setup failed")
      error
    end

primitive \nodoc\ _TestSSLSessionPair
  fun val apply(h: TestHelper): (SSL, SSL) ? =>
    """
    A handshaken client and server session from the standard test context,
    with no transport between them.
    """
    (let client, let server) = fresh(h)?
    _handshake(h, client, server)?
    (client, server)

  fun val fresh(h: TestHelper): (SSL, SSL) ? =>
    """
    A client and a server session from the standard test context, before any
    handshake.
    """
    (let client_session, let server_session) = _TestSSLDefaultSessions(h)?
    let client: SSL = consume client_session
    let server: SSL = consume server_session
    (client, server)

  fun val from_context(h: TestHelper, sslctx: SSLContext val): (SSL, SSL) ? =>
    """
    A handshaken client and server session from `sslctx`, with no transport
    between them.
    """
    let client: SSL =
      try
        sslctx.client()?
      else
        h.fail("failed getting ssl client session")
        error
      end
    let server: SSL =
      try
        sslctx.server()?
      else
        h.fail("failed getting ssl server session")
        error
      end
    _handshake(h, client, server)?
    (client, server)

  fun val _fresh_from(
    h: TestHelper,
    client_ctx: SSLContext val,
    server_ctx: SSLContext val,
    hostname: String = "")
    : (SSL, SSL) ?
  =>
    """
    A client session from `client_ctx` and a server session from `server_ctx`,
    before any handshake.
    """
    let client: SSL =
      try
        client_ctx.client(hostname)?
      else
        h.fail("failed getting ssl client session")
        error
      end
    let server: SSL =
      try
        server_ctx.server()?
      else
        h.fail("failed getting ssl server session")
        client.dispose()
        error
      end
    (client, server)

  fun val attempt(
    h: TestHelper,
    client_ctx: SSLContext val,
    server_ctx: SSLContext val,
    hostname: String = "")
    : (SSL, SSL, SSLReceiveResult, SSLReceiveResult) ?
  =>
    """
    A client session from `client_ctx` and a server session from `server_ctx`,
    handed each other's bytes until neither is still handshaking or the round
    cap stops it. A handshake that does not complete is a result to assert on
    rather than a reason to fail the test, which is what separates this from
    `apply`. `SSLAccepted` means the session did not finish handshaking.
    """
    (let client, let server) = _fresh_from(h, client_ctx, server_ctx, hostname)?
    (let cr, let sr) = _pump(client, server)
    (client, server, cr, sr)

  fun val _pump(
    client: SSL,
    server: SSL)
    : (SSLReceiveResult, SSLReceiveResult)
  =>
    """
    Hand each side's outgoing bytes to the other until neither is still
    handshaking, and return each side's `receive` result. Stops at
    `_max_rounds` whether or not the sessions settled, so a caller that needs
    them settled has to check.
    """
    var rounds: USize = 0
    var client_result: SSLReceiveResult = SSLAccepted
    var server_result: SSLReceiveResult = SSLAccepted

    while
      ((client_result is SSLAccepted) or
        (server_result is SSLAccepted)) and
        (rounds < _max_rounds())
    do
      rounds = rounds + 1
      let sr = _TestSSLTransfer(client, server)
      let cr = _TestSSLTransfer(server, client)
      if not (sr is SSLAccepted) then server_result = sr end
      if not (cr is SSLAccepted) then client_result = cr end
    end

    (client_result, server_result)

  fun val _max_rounds(): USize =>
    """
    How many rounds a handshake takes depends on the TLS version and the
    backend, so this is a backstop against a session that never settles, not a
    count anything should rely on.
    """
    20

  fun val _handshake(h: TestHelper, client: SSL, server: SSL) ? =>
    """
    Drive a handshake to completion by handing each side's outgoing bytes
    straight to the other. Reports the reason and raises an error if the
    handshake does not finish.
    """
    (let cr, let sr) = _pump(client, server)

    if (cr is SSLAccepted) or (sr is SSLAccepted) then
      h.fail("in memory SSL handshake did not finish")
      error
    end

    if (cr isnt SSLReady) or (sr isnt SSLReady) then
      h.fail("in memory SSL handshake did not reach SSLReady")
      error
    end

class \nodoc\ iso _TestSSLHandshakeInMemory is UnitTest
  """
  Two SSL sessions can complete a handshake with no transport between them by
  handing each side's outgoing bytes straight to the other, and application
  data written by one can be read by the other.

  The `after_dispose` tests all start from a handshaken pair. This test
  verifies that the pair really handshakes and really moves data, so when one
  of those tests fails, the session under test is at fault and not the
  harness.
  """
  fun name(): String => "net/ssl/SSL/handshake_in_memory"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    try
      client.write("hello")?
      _TestSSLTransfer(client, server)
    else
      h.fail("client could not send application data")
      client.dispose()
      server.dispose()
      return
    end

    match \exhaustive\ server.read()
    | let data: Array[U8] iso =>
      h.assert_eq[String]("hello", String.from_array(consume data))
    | None => h.fail("server read no application data")
    | SSLClosed => h.fail("server read returned SSLClosed")
    | SSLError => h.fail("server read returned SSLError")
    | InvalidOperation => h.fail("server read returned InvalidOperation")
    end

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLCreateClientNoAvailableProtocol is UnitTest
  """
  A client session whose context has no available protocol version reports
  `SSLError` immediately after construction.

  The context allows only TLS 1.2 and then disables it via options, leaving no
  version for the handshake to use. `SSL_do_handshake` fails during `_create`,
  and the session classifies that failure before the constructor returns.
  """
  fun name(): String => "net/ssl/SSL._create/no_available_protocol"

  fun apply(h: TestHelper) =>
    let ctx =
      try
        recover val
          SSLContext
            .> set_max_proto_version(TLS1u2Version())?
            .> allow_tls_v1_2(false)
        end
      else
        h.fail("ssl context setup failed")
        return
      end

    let client =
      try
        ctx.client()?
      else
        h.fail("client() raised on a misconfigured context")
        return
      end

    h.assert_true(
      client.receive("") is SSLError,
      "a client with no available protocol should report SSLError")

    client.dispose()

class \nodoc\ iso _TestSSLReceiveNonTLSBytes is UnitTest
  """
  A verifying session handed bytes that are not a TLS record reports
  `SSLError`.

  Those bytes fail the handshake at the SSL layer, which is where a chain that
  will not verify fails too. Reporting `SSLAuthFail` for them tells a caller
  the peer could not be authenticated when nothing about the peer's identity
  was in question.
  """
  fun name(): String => "net/ssl/SSL.receive/non_tls_bytes"

  fun apply(h: TestHelper) =>
    let client =
      try
        _TestSSLContext(h where authority = true, client_verify = true)?
          .client()?
      else
        h.fail("could not create a client session")
        return
      end

    let result = client.receive("NOT AN SSL HANDSHAKE\r\n")

    h.assert_true(
      result is SSLError,
      "bytes that are not a TLS record should report SSLError")

    client.dispose()

class \nodoc\ iso _TestSSLReceiveUntrustedChain is UnitTest
  """
  A verifying session whose peer presents a chain it does not trust reports
  `SSLAuthFail`.

  The client here trusts no authority at all, so the server's certificate has
  nothing to chain to.
  """
  fun name(): String => "net/ssl/SSL.receive/untrusted_chain"

  fun apply(h: TestHelper) =>
    (let client, let server, let cr, _) =
      try
        _TestSSLSessionPair.attempt(
          h,
          _TestSSLContext(h where client_verify = true)?,
          _TestSSLContext(h where cert = true)?)?
      else
        h.fail("could not create an SSL session pair")
        return
      end

    h.assert_true(
      cr is SSLAuthFail,
      "a chain the client does not trust should report SSLAuthFail")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLReceivePeerRejectedOurCert is UnitTest
  """
  A session whose peer rejects the certificate it presented reports
  `SSLError`.

  The client trusts no authority, so it rejects the server's certificate and
  sends an alert. What reaches the server is that alert, not a failure of the
  server's own verification, so the server has not failed to authenticate
  anyone.
  """
  fun name(): String => "net/ssl/SSL.receive/peer_rejected_our_cert"

  fun apply(h: TestHelper) =>
    (let client, let server, _, let sr) =
      try
        _TestSSLSessionPair.attempt(
          h,
          _TestSSLContext(h where client_verify = true)?,
          _TestSSLContext(
            h where cert = true, authority = true, server_verify = true)?)?
      else
        h.fail("could not create an SSL session pair")
        return
      end

    h.assert_true(
      sr is SSLError,
      "a peer that rejected our certificate should report SSLError")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLReceiveHostnameMismatch is UnitTest
  """
  A verifying session whose peer presents a certificate that is not valid for
  the hostname reports `SSLAuthFail`.

  The handshake itself succeeds here, because the certificate chains to an
  authority the client trusts. `SSL._verify_hostname` is what rejects it.
  """
  fun name(): String => "net/ssl/SSL.receive/hostname_mismatch"

  fun apply(h: TestHelper) =>
    (let client, let server, let cr, _) =
      try
        let client_ctx =
          _TestSSLContext(h where authority = true, client_verify = true)?
        let server_ctx = _TestSSLContext(h where cert = true)?
        _TestSSLSessionPair.attempt(
          h, client_ctx, server_ctx where hostname = "nomatch.example.com")?
      else
        h.fail("could not create an SSL session pair")
        return
      end

    h.assert_true(
      cr is SSLAuthFail,
      "a certificate that is not valid for the hostname should report " +
        "SSLAuthFail")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLReceiveNoSharedVersion is UnitTest
  """
  Two verifying sessions with no protocol version in common both report
  `SSLError`.

  Neither side got as far as a certificate, so neither failed to authenticate
  the other.
  """
  fun name(): String => "net/ssl/SSL.receive/no_shared_version"

  fun apply(h: TestHelper) =>
    (let client, let server, let cr, let sr) =
      try
        _TestSSLSessionPair.attempt(
          h,
          _TestSSLContext(
            h
            where authority = true,
              client_verify = true,
              max_proto = TLS1u2Version())?,
          _TestSSLContext(
            h
            where cert = true,
              authority = true,
              server_verify = true,
              min_proto = TLS1u3Version())?)?
      else
        h.fail("could not create an SSL session pair")
        return
      end

    h.assert_true(
      cr is SSLError,
      "a client with no protocol version in common should report SSLError")
    h.assert_true(
      sr is SSLError,
      "a server with no protocol version in common should report SSLError")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLReceiveNoPeerCertificate is UnitTest
  """
  A verifying session whose peer presents no certificate at all reports
  `SSLAuthFail`.

  This is a server that asked its client to authenticate and got nothing back.
  No chain was checked, so the verify result is `X509_V_OK` and the error queue
  is the only place the failure is reported.

  Both sides are pinned to TLS 1.3, which is what leaves the client `SSLReady`:
  it finishes its side of the handshake before the server's alert reaches it.
  Under TLS 1.2 the same exchange leaves the client in `SSLError`.
  """
  fun name(): String => "net/ssl/SSL.receive/no_peer_certificate"

  fun apply(h: TestHelper) =>
    (let client, let server, let cr, let sr) =
      try
        _TestSSLSessionPair.attempt(
          h,
          _TestSSLContext(
            h
            where authority = true,
              client_verify = true,
              min_proto = TLS1u3Version())?,
          _TestSSLContext(
            h
            where cert = true,
              authority = true,
              server_verify = true,
              min_proto = TLS1u3Version())?)?
      else
        h.fail("could not create an SSL session pair")
        return
      end

    h.assert_true(
      sr is SSLAuthFail,
      "a peer that presented no certificate should report SSLAuthFail")
    h.assert_true(
      cr is SSLReady,
      "the client accepted the server and finished before the alert")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLReceiveVerifyOffNeverAuthFails is UnitTest
  """
  A session created with verification off reports `SSLError` when its handshake
  fails, whatever its peer's certificate did.

  OpenSSL verifies the peer's chain even when the session is set not to enforce
  the result, so this client carries a failed verify result for a chain it was
  never asked to check. The server then rejects the empty certificate the
  client sends, and that alert is what fails the handshake. Reporting
  `SSLAuthFail` here would attribute the failure to a check the caller turned
  off.

  Each of the client's settings matters, and changing any one of them stops
  the test exercising the guard. Load an authority and the verify result is
  `X509_V_OK`. Give the client a certificate and it satisfies the server, so
  both sides reach `SSLReady`. Lift the TLS 1.2 cap and the client reaches
  `SSLReady` before the server's alert arrives.
  """
  fun name(): String => "net/ssl/SSL.receive/verify_off_never_auth_fails"

  fun apply(h: TestHelper) =>
    (let client, let server, let cr, _) =
      try
        _TestSSLSessionPair.attempt(
          h,
          _TestSSLContext(
            h where client_verify = false, max_proto = TLS1u2Version())?,
          _TestSSLContext(
            h where cert = true, authority = true, server_verify = true)?)?
      else
        h.fail("could not create an SSL session pair")
        return
      end

    h.assert_true(
      cr is SSLError,
      "a session that was not asked to verify should report SSLError")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLReceiveUnprovenCertificate is UnitTest
  """
  A verifying session whose peer presents a certificate it cannot prove it
  holds reports `SSLAuthFail`.

  A certificate is public, so presenting a copy of one is what an impersonator
  without the matching key does. The chain verifies — the certificate is
  genuine — but the handshake signature check fails, and the peer certificate
  is still stored in the session.

  Flipping a byte of the server's flight is what breaks the signature. It has
  to land inside the key exchange, past the certificate and short of the last
  message, and it needs TLS 1.2, where those messages are not encrypted.
  """
  fun name(): String => "net/ssl/SSL.receive/unproven_certificate"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair._fresh_from(
          h,
          _TestSSLContext(
            h
            where authority = true,
              client_verify = true,
              max_proto = TLS1u2Version())?,
          _TestSSLContext(
            h
            where cert = true,
              min_proto = TLS1u2Version(),
              max_proto = TLS1u2Version())?)?
      else
        h.fail("could not create an SSL session pair")
        return
      end

    try
      _TestSSLTransfer(client, server)

      let flight =
        match \exhaustive\ server.send()
        | let d: Array[U8] iso => consume d
        | None => error
        end
      let at = flight.size() - 80
      flight(at)? = flight(at)? xor 0xFF
      let cr = client.receive(consume flight)
      h.assert_true(
        cr is SSLAuthFail,
        "a certificate the peer cannot prove it holds should report " +
          "SSLAuthFail")
    else
      h.fail("could not corrupt the server's flight")
      client.dispose()
      server.dispose()
      return
    end

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestERRCodeFields is UnitTest
  """
  `_ERRLibrary.of` and `_ERRReason.of` take the library and the reason out of
  an error code the way each backend's `ERR_GET_LIB` and `ERR_GET_REASON` do.

  The words below are ones the backend produced for
  `SSL_R_PEER_DID_NOT_RETURN_A_CERTIFICATE`. A handshake exercises these two
  methods only against the backend it runs on, so a shift that is wrong for a
  different backend still passes one.
  """
  fun name(): String => "net/ssl/_ERRCode/fields"

  fun apply(h: TestHelper) =>
    let code: ULong =
      ifdef "openssl_3.0.x" or "openssl_4.0.x" then
        0x0A0000C7
      elseif "openssl_1.1.x" or "libressl" then
        0x14FFF0C7
      else
        compile_error "You must select an SSL version to use."
      end

    h.assert_eq[ULong](
      _ERRLibrary.ssl(),
      _ERRLibrary.of(code),
      "the library the recorded word carries")
    h.assert_eq[ULong](
      _ERRReason.peer_did_not_return_a_certificate(),
      _ERRReason.of(code),
      "the reason the recorded word carries")

    // 199 is `ASN1_R_UNKNOWN_SIGNATURE_ALGORITHM` in libcrypto's ASN.1, which
    // is library 13. The reason alone does not say a peer sent no certificate.
    let asn1: ULong =
      ifdef "openssl_3.0.x" or "openssl_4.0.x" then
        (13 << 23) or 199
      elseif "openssl_1.1.x" or "libressl" then
        (13 << 24) or 199
      else
        compile_error "You must select an SSL version to use."
      end

    h.assert_eq[ULong](
      13, _ERRLibrary.of(asn1), "the library an ASN.1 word carries")
    h.assert_eq[ULong](
      _ERRReason.peer_did_not_return_a_certificate(),
      _ERRReason.of(asn1),
      "an ASN.1 word can carry the same reason number")

class \nodoc\ iso _TestSSLReceiveUntrustedClientChain is UnitTest
  """
  A verifying server whose client presents a chain it does not trust reports
  `SSLAuthFail`.

  The same failure as `net/ssl/SSL.receive/untrusted_chain`, from the other
  side of the connection.
  """
  fun name(): String => "net/ssl/SSL.receive/untrusted_client_chain"

  fun apply(h: TestHelper) =>
    (let client, let server, _, let sr) =
      try
        _TestSSLSessionPair.attempt(
          h,
          _TestSSLContext(h where cert = true, client_verify = false)?,
          _TestSSLContext(h where cert = true, server_verify = true)?)?
      else
        h.fail("could not create an SSL session pair")
        return
      end

    h.assert_true(
      sr is SSLAuthFail,
      "a client chain the server does not trust should report SSLAuthFail")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLReceiveALPNFatalWithQueuedError is UnitTest
  """
  An ALPN resolver that rejects the protocol reports the same state whether
  or not it leaves a system error on the thread's error queue.

  A clean `ALPNFatal` puts `ssl routines::no application protocol` on the
  queue, so `SSL_get_error` returns `SSL_ERROR_SSL`. A resolver that
  fails an OpenSSL call first puts a system error as the older entry, and
  `SSL_get_error` returns `SSL_ERROR_SYSCALL` instead. Both error codes must
  reach the same classification logic, or the reported state depends on what
  the resolver happened to call.
  """
  fun name(): String => "net/ssl/SSL.receive/alpn_fatal_with_queued_error"

  fun apply(h: TestHelper) =>
    let auth = FileAuth(h.env.root)

    let clean_result = _attempt(h, _TestALPNFatalResolver)
    let dirty_result =
      _attempt(h, _TestALPNContaminatingFatalResolver(auth))

    h.assert_true(
      clean_result is dirty_result,
      "queue contamination from the resolver changed the server's result")

  fun _attempt(
    h: TestHelper,
    resolver: ALPNProtocolResolver val)
    : SSLReceiveResult
  =>
    let auth = FileAuth(h.env.root)
    let sslctx =
      try
        recover val
          SSLContext
            .> set_authority(FilePath(auth, "assets/cert.pem"))?
            .> set_cert(
                FilePath(auth, "assets/cert.pem"),
                FilePath(auth, "assets/key.pem"))?
            .> set_client_verify(false)
            .> set_server_verify(false)
            .> alpn_set_client_protocols(["h2"])
            .> alpn_set_resolver(resolver)
        end
      else
        h.fail("ssl context setup failed")
        return SSLError
      end

    (let client, let server, _, let sr) =
      try
        _TestSSLSessionPair.attempt(h, sslctx, sslctx)?
      else
        h.fail("could not create an SSL session pair")
        return SSLError
      end

    client.dispose()
    server.dispose()
    sr

class \nodoc\ iso _TestSSLFailedHandshakeDoesNotAffectLaterRead is UnitTest
  """
  A handshake that fails on one session does not put a session that reads
  afterwards into `SSLError`.

  OpenSSL's error queue belongs to the thread, not to the session, and
  `SSL_get_error` describes the call it is given only when that queue was empty
  beforehand. A failed handshake leaves entries on it. A session that reads on
  the same thread after that failure, with nothing to read, should stay
  `SSLReady`. If it reports `SSLError` instead, a consumer branching on the
  state closes a connection that is working.
  """
  fun name(): String =>
    "net/ssl/SSL/failed_handshake_does_not_affect_later_read"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    // A handshake clears the error queue, so the failure has to be staged after
    // the healthy pair handshakes, not before.
    // A verifying session takes entries off the queue to classify its own
    // failure, so the session staging this one must not be verifying.
    (let failed, let failed_peer) =
      try
        _TestSSLSessionPair._fresh_from(
          h,
          _TestSSLContext(h where client_verify = false)?,
          _TestSSLContext(h where cert = true)?)?
      else
        h.fail("could not create an SSL session pair")
        client.dispose()
        server.dispose()
        return
      end

    // These bytes are not a valid TLS record, so the handshake fails.
    let fr = failed.receive("NOT AN SSL HANDSHAKE\r\n")
    h.assert_true(
      fr is SSLError,
      "non-handshake bytes should report SSLError")

    // With an empty queue this test cannot fail whatever `read` does, so check
    // the failure left something on it.
    h.assert_ne[ULong](
      0,
      @ERR_peek_error(),
      "the failed handshake left nothing on the thread's error queue")

    // Nothing was written to server, so this read has nothing to return.
    match \exhaustive\ server.read()
    | let data: Array[U8] iso =>
      h.fail(
        "a read with no data returned " +
          (consume data).size().string() + " bytes")
    | None => None
    | SSLClosed =>
      h.fail("a session with nothing to read reported SSLClosed")
    | SSLError =>
      h.fail("a session with nothing to read reported SSLError")
    | InvalidOperation =>
      h.fail("a session with nothing to read reported InvalidOperation")
    end

    failed.dispose()
    failed_peer.dispose()
    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLReadReportsGenuineError is UnitTest
  """
  A read whose record will not decrypt reports `SSLError`.

  Emptying the thread's error queue before each OpenSSL call must not cost a
  session the errors that are its own. Flipping a byte of an encrypted record
  breaks its authentication tag, which gives the read a real failure to report.
  """
  fun name(): String => "net/ssl/SSL/read_reports_genuine_error"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    try
      _TestSSLCorruptRecord(client, server, "hello")?
    else
      h.fail("could not send a corrupted record")
      client.dispose()
      server.dispose()
      return
    end

    match \exhaustive\ server.read()
    | let data: Array[U8] iso =>
      h.fail(
        "a corrupted record produced " +
          (consume data).size().string() + " bytes")
    | None =>
      h.fail("a corrupted record returned None instead of SSLError")
    | SSLClosed =>
      h.fail("a corrupted record returned SSLClosed instead of SSLError")
    | SSLError => None
    | InvalidOperation =>
      h.fail("a corrupted record returned InvalidOperation instead of SSLError")
    end

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLReadAfterErrorReturnsOnlyDecryptedBytes is UnitTest
  """
  A read that fails partway through an `expect` block does not hand back the
  part of the block no decrypt ever wrote.

  Fifty bytes and a read of a hundred leave half a block buffered. The read
  that meets the broken record fails with those fifty still there, so a read
  of a hundred must return `None` and a read of fifty must return the fifty.
  """
  fun name(): String =>
    "net/ssl/SSL.read/after_error_returns_only_decrypted_bytes"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    // Half of the hundred the reads below request, so the first of them
    // buffers these bytes and returns nothing.
    try
      client.write(recover val Array[U8].init('A', 50) end)?
      _TestSSLTransfer(client, server)
    else
      h.fail("client could not send application data")
      client.dispose()
      server.dispose()
      return
    end

    match \exhaustive\ server.read(100)
    | let data: Array[U8] iso =>
      h.fail(
        "50 bytes satisfied a read of 100 and produced " +
          (consume data).size().string() + " bytes")
      client.dispose()
      server.dispose()
      return
    | None => None
    | SSLClosed =>
      h.fail("read returned SSLClosed before any close")
      client.dispose()
      server.dispose()
      return
    | SSLError =>
      h.fail("read returned SSLError before any corruption")
      client.dispose()
      server.dispose()
      return
    | InvalidOperation =>
      h.fail("read returned InvalidOperation before any corruption")
      client.dispose()
      server.dispose()
      return
    end

    try
      _TestSSLCorruptRecord(
        client, server, recover val Array[U8].init('B', 50) end)?
    else
      h.fail("could not send a corrupted record")
      client.dispose()
      server.dispose()
      return
    end

    match \exhaustive\ server.read(100)
    | let data: Array[U8] iso =>
      h.fail(
        "a corrupted record produced " +
          (consume data).size().string() + " bytes")
    | None =>
      h.fail("a corrupted record returned None instead of SSLError")
    | SSLClosed =>
      h.fail("a corrupted record returned SSLClosed instead of SSLError")
    | SSLError => None
    | InvalidOperation =>
      h.fail("a corrupted record returned InvalidOperation instead of SSLError")
    end

    match \exhaustive\ server.read(100)
    | let data: Array[U8] iso =>
      h.fail(
        "a read of 100 returned " +
          (consume data).size().string() +
          " bytes with only 50 decrypted")
    | None => None
    | SSLClosed =>
      h.fail("read returned SSLClosed on an errored session")
    | SSLError => None
    | InvalidOperation =>
      h.fail("read returned InvalidOperation on an errored session")
    end

    // A read with no `expect` decrypts before it hands anything over, so on a
    // failed session it returns `None` and leaves the fifty where they are.
    match \exhaustive\ server.read()
    | let data: Array[U8] iso =>
      h.fail(
        "a read with no expect returned " +
          (consume data).size().string() +
          " bytes from a failed session")
    | None => None
    | SSLClosed =>
      h.fail("read returned SSLClosed on an errored session")
    | SSLError => None
    | InvalidOperation =>
      h.fail("read returned InvalidOperation on an errored session")
    end

    match \exhaustive\ server.read(50)
    | let data: Array[U8] iso =>
      h.assert_array_eq[U8](
        recover val Array[U8].init('A', 50) end,
        consume data,
        "a read of 50 returned something other than the 50 decrypted bytes")
    | None =>
      h.fail("a read of 50 returned none of the 50 bytes that were decrypted")
    | SSLClosed =>
      h.fail("a read of 50 returned SSLClosed")
    | SSLError =>
      h.fail("a read of 50 returned SSLError")
    | InvalidOperation =>
      h.fail("a read of 50 returned InvalidOperation")
    end

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLReadWithBufferedBytes is UnitTest
  """
  A read with no `expect` returns `None` when it decrypts nothing, whatever is
  already buffered. A read with an `expect` at or below how many are buffered
  returns all of them, not `expect` of them.
  """
  fun name(): String => "net/ssl/SSL.read/with_buffered_bytes"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    try
      client.write(recover val Array[U8].init('A', 50) end)?
      _TestSSLTransfer(client, server)
    else
      h.fail("client could not send application data")
      client.dispose()
      server.dispose()
      return
    end

    match \exhaustive\ server.read(100)
    | let data: Array[U8] iso =>
      h.fail(
        "50 bytes satisfied a read of 100 and produced " +
          (consume data).size().string() + " bytes")
      client.dispose()
      server.dispose()
      return
    | None => None
    | SSLClosed =>
      h.fail("read returned SSLClosed on a healthy session")
      client.dispose()
      server.dispose()
      return
    | SSLError =>
      h.fail("read returned SSLError on a healthy session")
      client.dispose()
      server.dispose()
      return
    | InvalidOperation =>
      h.fail("read returned InvalidOperation on a healthy session")
      client.dispose()
      server.dispose()
      return
    end

    match \exhaustive\ server.read()
    | let data: Array[U8] iso =>
      h.fail(
        "a read with no expect returned " +
          (consume data).size().string() +
          " bytes from a call that decrypted nothing")
    | None => None
    | SSLClosed =>
      h.fail("read returned SSLClosed on a healthy session")
    | SSLError =>
      h.fail("read returned SSLError on a healthy session")
    | InvalidOperation =>
      h.fail("read returned InvalidOperation on a healthy session")
    end

    match \exhaustive\ server.read(20)
    | let data: Array[U8] iso =>
      h.assert_eq[USize](
        50,
        (consume data).size(),
        "a read of 20 against 50 buffered bytes returned")
    | None => h.fail("a read of 20 returned None with 50 bytes buffered")
    | SSLClosed => h.fail("a read of 20 returned SSLClosed")
    | SSLError => h.fail("a read of 20 returned SSLError")
    | InvalidOperation => h.fail("a read of 20 returned InvalidOperation")
    end

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLReadNoExpectAfterError is UnitTest
  """
  A read with no `expect` on a failed session returns `None` and leaves nothing
  behind that a later read hands out.

  The read of one byte at the end returns whatever is buffered, so it reports
  what the failed read left.
  """
  fun name(): String => "net/ssl/SSL.read/no_expect_after_error"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    try
      _TestSSLCorruptRecord(client, server, "hello")?
    else
      h.fail("could not send a corrupted record")
      client.dispose()
      server.dispose()
      return
    end

    match \exhaustive\ server.read()
    | let data: Array[U8] iso =>
      h.fail(
        "a corrupted record produced " +
          (consume data).size().string() + " bytes")
    | None => None
    | SSLClosed =>
      h.fail("read returned SSLClosed on an errored session")
    | SSLError => None
    | InvalidOperation =>
      h.fail("read returned InvalidOperation on an errored session")
    end

    // A read of one byte returns the buffer whole if the failed read left
    // anything in it, so a returned array gives the size that was left.
    match \exhaustive\ server.read(1)
    | let data: Array[U8] iso =>
      h.fail(
        "a read of 1 returned " +
          (consume data).size().string() +
          " bytes with none decrypted")
    | None => None
    | SSLClosed =>
      h.fail("read returned SSLClosed on an errored session")
    | SSLError => None
    | InvalidOperation =>
      h.fail("read returned InvalidOperation on an errored session")
    end

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLReadPendingExceedsExpect is UnitTest
  """
  A read that draws on more bytes than it requested takes only what it
  requested.

  Fifty bytes are written at once and no ciphertext moves between the two
  reads, so the second draws on bytes OpenSSL had already decrypted.
  """
  fun name(): String => "net/ssl/SSL.read/pending_exceeds_expect"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    try
      client.write(recover val Array[U8].init('A', 50) end)?
      _TestSSLTransfer(client, server)
    else
      h.fail("client could not send application data")
      client.dispose()
      server.dispose()
      return
    end

    match \exhaustive\ server.read(20)
    | let data: Array[U8] iso =>
      h.assert_eq[USize](20, (consume data).size(), "read(20) returned")
    | None => h.fail("read(20) returned None with 50 bytes available")
    | SSLClosed => h.fail("read(20) returned SSLClosed")
    | SSLError => h.fail("read(20) returned SSLError")
    | InvalidOperation => h.fail("read(20) returned InvalidOperation")
    end

    match \exhaustive\ server.read(10)
    | let data: Array[U8] iso =>
      h.assert_eq[USize](10, (consume data).size(), "read(10) returned")
    | None => h.fail("read(10) returned None with 30 bytes pending")
    | SSLClosed => h.fail("read(10) returned SSLClosed")
    | SSLError => h.fail("read(10) returned SSLError")
    | InvalidOperation => h.fail("read(10) returned InvalidOperation")
    end

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLReadOnAuthFail is UnitTest
  """
  `read` on a session that failed authentication returns `SSLError` and does
  not lose the authentication failure.

  The `receive` call that drove the handshake reported `SSLAuthFail`. A
  subsequent `read` with no buffered data returns `SSLError`.
  """
  fun name(): String => "net/ssl/SSL.read/on_auth_fail"

  fun apply(h: TestHelper) =>
    (let client, let server, let cr, _) =
      try
        let client_ctx =
          _TestSSLContext(h where authority = true, client_verify = true)?
        let server_ctx = _TestSSLContext(h where cert = true)?
        _TestSSLSessionPair.attempt(
          h, client_ctx, server_ctx where hostname = "nomatch.example.com")?
      else
        h.fail("could not create an SSL session pair")
        return
      end

    h.assert_true(
      cr is SSLAuthFail,
      "the client should report SSLAuthFail after a hostname mismatch")

    h.assert_true(
      client.read() is SSLError,
      "read() on a failed session should return SSLError")

    h.assert_true(
      client.read(10) is SSLError,
      "read(10) on a failed session should return SSLError")

    h.assert_true(
      client.receive("") is SSLAuthFail,
      "receive should still report SSLAuthFail after reads")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLALPNSelectedOnAuthFail is UnitTest
  """
  `alpn_selected` on a session in `SSLAuthFail` returns `None`, even though
  the session negotiated a protocol during the handshake.

  The negotiated protocol is peer-supplied data. Returning it from a session
  whose peer failed authentication leaks the peer's selection.
  """
  fun name(): String => "net/ssl/SSL.alpn_selected/on_auth_fail"

  fun apply(h: TestHelper) =>
    let auth = FileAuth(h.env.root)
    let client_ctx =
      try
        recover val
          SSLContext
            .> set_authority(FilePath(auth, "assets/cert.pem"))?
            .> alpn_set_client_protocols(["h2"])
        end
      else
        h.fail("client ssl context setup failed")
        return
      end
    let server_ctx =
      try
        recover val
          SSLContext
            .> set_cert(
                FilePath(auth, "assets/cert.pem"),
                FilePath(auth, "assets/key.pem"))?
            .> alpn_set_resolver(ALPNStandardProtocolResolver(["h2"]))
        end
      else
        h.fail("server ssl context setup failed")
        return
      end

    (let client, let server, let cr, _) =
      try
        _TestSSLSessionPair.attempt(
          h, client_ctx, server_ctx where hostname = "nomatch.example.com")?
      else
        h.fail("could not create an SSL session pair")
        return
      end

    h.assert_true(
      cr is SSLAuthFail,
      "the client should report SSLAuthFail after a hostname mismatch")

    h.assert_true(
      try (server.alpn_selected() as String) == "h2" else false end,
      "the server should have negotiated h2")

    h.assert_true(
      client.alpn_selected() is None,
      "alpn_selected() on a failed session should return None")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLALPNSelectedOnError is UnitTest
  """
  `alpn_selected` on a session in `SSLError` returns `None`, even though
  the session negotiated a protocol before it failed.
  """
  fun name(): String => "net/ssl/SSL.alpn_selected/on_error"

  fun apply(h: TestHelper) =>
    let auth = FileAuth(h.env.root)
    let client_ctx =
      try
        recover val
          SSLContext
            .> set_cert(
                FilePath(auth, "assets/cert.pem"),
                FilePath(auth, "assets/key.pem"))?
            .> set_authority(FilePath(auth, "assets/cert.pem"))?
            .> alpn_set_client_protocols(["h2"])
        end
      else
        h.fail("client ssl context setup failed")
        return
      end
    let server_ctx =
      try
        recover val
          SSLContext
            .> set_cert(
                FilePath(auth, "assets/cert.pem"),
                FilePath(auth, "assets/key.pem"))?
            .> set_authority(FilePath(auth, "assets/cert.pem"))?
            .> alpn_set_resolver(ALPNStandardProtocolResolver(["h2"]))
        end
      else
        h.fail("server ssl context setup failed")
        return
      end

    (let client, let server, let cr, _) =
      try
        _TestSSLSessionPair.attempt(h, client_ctx, server_ctx)?
      else
        h.fail("could not create an SSL session pair")
        return
      end

    h.assert_true(
      cr is SSLReady,
      "the client should report SSLReady after a successful handshake")

    h.assert_true(
      try (server.alpn_selected() as String) == "h2" else false end,
      "the server should have negotiated h2 before the failure")

    try
      _TestSSLCorruptRecord(client, server, "hello")?
    else
      h.fail("could not send a corrupted record")
      client.dispose()
      server.dispose()
      return
    end

    let sr = server.read()

    h.assert_true(
      sr is SSLError,
      "a corrupted record should report SSLError from read")

    h.assert_true(
      server.alpn_selected() is None,
      "alpn_selected() on a failed session should return None")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLReceiveOnAuthFail is UnitTest
  """
  `receive` on a session in `SSLAuthFail` does nothing.

  The session rejected its peer's identity. Accepting more ciphertext from
  that peer would let `read` decrypt data the application should never see.
  """
  fun name(): String => "net/ssl/SSL.receive/on_auth_fail"

  fun apply(h: TestHelper) =>
    (let client, let server, let cr, let sr) =
      try
        let client_ctx =
          _TestSSLContext(h where authority = true, client_verify = true)?
        let server_ctx = _TestSSLContext(h where cert = true)?
        _TestSSLSessionPair.attempt(
          h, client_ctx, server_ctx where hostname = "nomatch.example.com")?
      else
        h.fail("could not create an SSL session pair")
        return
      end

    h.assert_true(
      cr is SSLAuthFail,
      "the client should report SSLAuthFail after a hostname mismatch")
    h.assert_true(
      sr is SSLReady,
      "the server should report SSLReady")

    try
      server.write("should not be received")?
      while true do
        match \exhaustive\ server.send()
        | let d: Array[U8] iso => client.receive(consume d)
        | None => break
        end
      end
    else
      h.fail("server could not produce ciphertext")
      client.dispose()
      server.dispose()
      return
    end

    h.assert_true(
      client.read() is SSLError,
      "data received after auth failure should not be readable")

    h.assert_true(
      client.receive("") is SSLAuthFail,
      "the session should still report SSLAuthFail")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLReceiveOnError is UnitTest
  """
  `receive` on a session in `SSLError` does nothing.

  Once a session has failed, accepting more ciphertext from its peer is
  pointless: the session cannot decrypt it and has no use for it.
  """
  fun name(): String => "net/ssl/SSL.receive/on_error"

  fun apply(h: TestHelper) =>
    let client =
      try
        _TestSSLContext(h where authority = true, client_verify = true)?
          .client()?
      else
        h.fail("could not create a client session")
        return
      end

    let r1 = client.receive("NOT AN SSL HANDSHAKE\r\n")

    h.assert_true(
      r1 is SSLError,
      "non-TLS bytes should report SSLError")

    let r2 = client.receive("more data after error")

    h.assert_true(
      r2 is SSLError,
      "receive should still report SSLError after a second receive")

    client.dispose()

class \nodoc\ iso _TestSSLDisposeBeforeHandshake is UnitTest
  """
  A session disposed before its handshake finishes is inert: `read` returns
  `InvalidOperation`, `send` returns `None`, and `receive` returns
  `InvalidOperation`.

  A fresh client session has a ClientHello waiting to go out, so `send`
  returning `None` after the dispose is the disposed check and not an empty
  BIO.
  """
  fun name(): String => "net/ssl/SSL.dispose/before_handshake"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair.fresh(h)?
      else
        h.fail("could not create an SSL session pair")
        return
      end

    h.assert_true(
      client.send() isnt None,
      "a fresh client session should have a ClientHello to send")

    client.dispose()

    h.assert_true(
      client.read() is InvalidOperation,
      "read() on a disposed session should return InvalidOperation")
    h.assert_true(
      client.send() is None,
      "send() on a disposed session should return None")

    h.assert_true(
      client.receive("bytes that will never be decrypted") is InvalidOperation,
      "receive on a disposed session should return InvalidOperation")

    server.dispose()

class \nodoc\ iso _TestSSLDisposeTwice is UnitTest
  """
  Disposing a session twice does not free the session or its BIOs twice, and
  the session is still inert afterwards.
  """
  fun name(): String => "net/ssl/SSL.dispose/twice"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    client.dispose()
    client.dispose()

    h.assert_true(
      client.read() is InvalidOperation,
      "read() on a disposed session should return InvalidOperation")
    h.assert_true(
      client.send() is None,
      "send() on a disposed session should return None")

    server.dispose()

class \nodoc\ iso _TestSSLReadAfterDispose is UnitTest
  """
  `read` on a disposed session returns `InvalidOperation` instead of passing
  a null `SSL*` to `SSL_pending`.
  """
  fun name(): String => "net/ssl/SSL.read/after_dispose"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    client.dispose()

    h.assert_true(
      client.read() is InvalidOperation,
      "read() on a disposed session should return InvalidOperation")
    h.assert_true(
      client.read(4) is InvalidOperation,
      "read(4) on a disposed session should return InvalidOperation")

    server.dispose()

class \nodoc\ iso _TestSSLReadAfterDisposeWithBufferedFrame is UnitTest
  """
  A session holding decrypted bytes from an incomplete `expect` frame returns
  `InvalidOperation` from `read` once it is disposed, rather than handing
  those bytes back.

  This is the one post-dispose read that did not crash before the fix. With
  at least `expect` bytes already buffered, `read` returns them without
  touching the freed session.
  """
  fun name(): String => "net/ssl/SSL.read/after_dispose_with_buffered_frame"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    try
      client.write("ab")?
      _TestSSLTransfer(client, server)
    else
      h.fail("client could not send application data")
      client.dispose()
      server.dispose()
      return
    end

    h.assert_true(
      server.read(4) is None,
      "two bytes should not satisfy read(4)")

    server.dispose()

    h.assert_true(
      server.read(2) is InvalidOperation,
      "read(2) on a disposed session should return " +
        "InvalidOperation, even with two bytes already buffered")

    client.dispose()

class \nodoc\ iso _TestSSLReceiveAfterDispose is UnitTest
  """
  `receive` on a disposed session returns `InvalidOperation` instead of
  writing into a freed BIO.
  """
  fun name(): String => "net/ssl/SSL.receive/after_dispose"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    server.dispose()

    h.assert_true(
      server.receive("bytes that will never be decrypted") is InvalidOperation,
      "receive on a disposed session should return InvalidOperation")

    h.assert_true(
      server.read() is InvalidOperation,
      "read on a disposed session should return InvalidOperation")

    client.dispose()

class \nodoc\ iso _TestSSLSendAfterDisposeReturnsNone is UnitTest
  """
  `send` on a disposed session returns `None` instead of reading a freed BIO.

  The session has encrypted bytes waiting when it is disposed, so `None`
  here is the disposed check and not an empty BIO.
  """
  fun name(): String => "net/ssl/SSL.send/after_dispose_returns_none"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    try
      client.write("data")?
    else
      h.fail("client could not write application data")
      client.dispose()
      server.dispose()
      return
    end

    h.assert_true(
      client.send() isnt None,
      "a session that has just written should have bytes to send")

    client.dispose()

    h.assert_true(
      client.send() is None,
      "send() on a disposed session should return None")

    server.dispose()

class \nodoc\ iso _TestSSLWriteAfterDispose is UnitTest
  """
  `write` on a disposed session does nothing and does not raise. Being disposed
  is not an error: `write` raises when the handshake is not complete or when
  `SSL_write` cannot encrypt, neither of which is what happened here.
  """
  fun name(): String => "net/ssl/SSL.write/after_dispose"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    client.dispose()

    try
      client.write("data")?
    else
      h.fail("write() on a disposed session should not raise an error")
    end

    h.assert_true(
      client.send() is None,
      "write() on a disposed session should not queue anything to send")

    server.dispose()

class \nodoc\ iso _TestSSLALPNSelectedAfterDispose is UnitTest
  """
  `alpn_selected` on a disposed session returns `None` rather than the
  protocol the session negotiated. The session negotiates one before it is
  disposed, so the `None` afterwards is the disposed check and not the absence
  of ALPN.

  This test passes with and without the disposed check on OpenSSL, whose
  `SSL_get0_alpn_selected` checks its own `SSL*` for null. We do not rely on
  that, and LibreSSL has not been checked, so the disposed check is what keeps
  the return value from depending on the backend.
  """
  fun name(): String => "net/ssl/SSL.alpn_selected/after_dispose"

  fun apply(h: TestHelper) =>
    let sslctx =
      try
        _TestALPNContext(h, ALPNStandardProtocolResolver(["h2"]))?
      else
        return
      end

    (let client, let server) =
      try
        _TestSSLSessionPair.from_context(h, sslctx)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    match \exhaustive\ client.alpn_selected()
    | let protocol: ALPNProtocolName =>
      h.assert_eq[String]("h2", protocol)
    | None =>
      h.fail("the client did not negotiate an ALPN protocol")
    end

    client.dispose()

    h.assert_true(
      client.alpn_selected() is None,
      "alpn_selected() on a disposed session should return None")

    server.dispose()

class \nodoc\ iso _TestSSLContextDisposeTwice is UnitTest
  """
  Disposing a context twice does not free it twice, and the context is still
  inert afterwards.
  """
  fun name(): String => "net/ssl/SSLContext.dispose/twice"

  fun apply(h: TestHelper) =>
    let ctx = SSLContext

    ctx.dispose()
    ctx.dispose()

    // A live context accepts this protocol list, so a `false` here is the
    // context still being disposed and not a list it rejected.
    h.assert_false(
      ctx.alpn_set_client_protocols(["h2"]),
      "a disposed context should still be inert after a second dispose()")

class \nodoc\ iso _TestSSLContextALPNSetResolverAfterDispose is UnitTest
  """
  `alpn_set_resolver` on a disposed context returns `false` rather than passing
  a null `SSL_CTX*` to `SSL_CTX_set_alpn_select_cb`, which dereferences it on
  every backend.

  The live context returns `true`, so the `false` afterwards is the disposed
  check and not a resolver the context rejected.
  """
  fun name(): String => "net/ssl/SSLContext.alpn_set_resolver/after_dispose"

  fun apply(h: TestHelper) =>
    let resolver = ALPNStandardProtocolResolver(["h2"])
    let ctx = SSLContext

    h.assert_true(
      ctx.alpn_set_resolver(resolver),
      "alpn_set_resolver() on a live context should return true")

    ctx.dispose()

    h.assert_false(
      ctx.alpn_set_resolver(resolver),
      "alpn_set_resolver() on a disposed context should return false")

class \nodoc\ val _TestALPNFatalResolver is ALPNProtocolResolver
  fun box resolve(advertised: Array[ALPNProtocolName] val): ALPNMatchResult =>
    ALPNFatal

class \nodoc\ val _TestALPNContaminatingFatalResolver
  is ALPNProtocolResolver
  """
  Rejects every advertisement and leaves a system error on the thread's error
  queue. The failed `set_cert` call inside `resolve` pushes the error;
  OpenSSL then pushes its own `ssl routines::no application protocol` entry
  on top. Because the system error is the older one, `SSL_get_error` returns
  `SSL_ERROR_SYSCALL` instead of `SSL_ERROR_SSL`.
  """
  let _auth: FileAuth

  new val create(auth: FileAuth) =>
    _auth = auth

  fun box resolve(advertised: Array[ALPNProtocolName] val): ALPNMatchResult =>
    let ctx = SSLContext
    try
      ctx.set_cert(
        FilePath(_auth, "/nonexistent/cert.pem"),
        FilePath(_auth, "/nonexistent/key.pem"))?
    end
    ctx.dispose()
    ALPNFatal

class \nodoc\ val _TestALPNFixedResolver is ALPNProtocolResolver
  """
  Resolves every advertisement to one name, whatever the client advertised.
  """
  let _protocol: String

  new val create(protocol: String) =>
    _protocol = protocol

  fun box resolve(advertised: Array[ALPNProtocolName] val): ALPNMatchResult =>
    _protocol

class \nodoc\ iso _TestSSLContextALPNFallbackToClientProtocol is UnitTest
  """
  A resolver that falls back to the client's first advertised protocol
  negotiates it.

  `ALPNStandardProtocolResolver` takes that name from the array the select
  callback built out of a copy of the wire buffer, not from its own `supported`
  list. It is the name the callback has no lifetime for, and the one the fix has
  to point back into the buffer OpenSSL passed in.

  The resolver's `supported` list shares nothing with what the client
  advertises, so the fallback is the only way it can select anything.

  The pointer moving is not observable from here. Every supported backend copies
  the bytes out of `*out` inside the callback's own C frame, so the pointer the
  callback handed over before this change was still live when it was read. What
  this drives is the path, and what it pins is that the fallback still
  negotiates once the pointer points into the client's buffer.
  """
  fun name(): String =>
    "net/ssl/SSLContext.alpn_set_resolver/fallback_to_client_protocol"

  fun apply(h: TestHelper) ? =>
    let ctx = _TestALPNContext(h, ALPNStandardProtocolResolver(["spdy/1"]))?
    (let client, let server) = _TestSSLSessionPair.from_context(h, ctx)?

    match \exhaustive\ client.alpn_selected()
    | let protocol: ALPNProtocolName =>
      h.assert_eq[String](
        "h2", protocol, "the client's own protocol should be selected")
    | None =>
      h.fail("the client did not negotiate an ALPN protocol")
    end

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLContextALPNUnadvertisedProtocolFails is UnitTest
  """
  A server whose resolver returns a protocol the client did not advertise stops
  handshaking on the client's first flight.

  The callback may only hand OpenSSL a pointer into the buffer the client sent.
  A name that is nowhere in that buffer has no such pointer, and a server that
  selects a protocol the client never offered is wrong to begin with.

  The assertion is on the server, and after a single transfer. An OpenSSL client
  refuses an unadvertised protocol on its own, a round or two later, so a test
  that waited for the whole handshake to fail would pass whether or not the
  server refused. It would be measuring the client.
  """
  fun name(): String =>
    "net/ssl/SSLContext.alpn_set_resolver/unadvertised_protocol_fails"

  fun apply(h: TestHelper) ? =>
    let ctx = _TestALPNContext(h, _TestALPNFixedResolver("spdy/1"))?
    let client: SSL = ctx.client()?
    let server: SSL = ctx.server()?

    // The client's opening flight carries the protocols it advertises. Handing
    // it to the server is what runs the resolver.
    let sr = _TestSSLTransfer(client, server)

    h.assert_true(
      sr is SSLError,
      "the server should refuse a protocol the client did not advertise")

    client.dispose()
    server.dispose()

class \nodoc\ val _TestALPNTrackedResolver is ALPNProtocolResolver
  """
  Resolves every advertisement to "h2" and reports its own collection by writing
  a byte through `_collected`, a raw pointer into an array a live
  `_TestALPNResolverTracker` holds.
  """
  let _collected: Pointer[U8] tag

  new val create(collected: Pointer[U8] tag) =>
    _collected = collected

  fun box resolve(advertised: Array[ALPNProtocolName] val): ALPNMatchResult =>
    "h2"

  fun _final() =>
    @memset(_collected, I32(1), USize(1))

class \nodoc\ _TestALPNResolverTracker
  """
  Reports whether the resolver it hands out has been garbage collected.
  """
  embed _flag: Array[U8] = Array[U8].init(0, 1)

  fun box resolver(): _TestALPNTrackedResolver =>
    """
    A resolver that reports its collection to this tracker. The tracker does not
    hold a reference to it: whether something else does is what the tests
    measure.
    """
    _TestALPNTrackedResolver(_flag.cpointer())

  fun box collected(): Bool =>
    // A one element array cannot fail to index. If it somehow did, treat it as
    // collected rather than as the resolver still being alive.
    try _flag(0)? != 0 else true end

primitive \nodoc\ _TestALPNContext
  fun apply(h: TestHelper, resolver: ALPNProtocolResolver val): SSLContext val ?
  =>
    """
    A context that both advertises and resolves the "h2" protocol, so a session
    pair made from it negotiates ALPN.
    """
    let auth = FileAuth(h.env.root)
    try
      recover val
        SSLContext
          .> set_authority(FilePath(auth, "assets/cert.pem"))?
          .> set_cert(
              FilePath(auth, "assets/cert.pem"),
              FilePath(auth, "assets/key.pem"))?
          .> set_client_verify(false)
          .> set_server_verify(false)
          .> alpn_set_client_protocols(["h2"])
          .> alpn_set_resolver(resolver)
      end
    else
      h.fail("ssl context setup failed")
      error
    end

actor \nodoc\ _TestALPNResolverContextRooting
  """
  Pony collects between behaviors, so the context is built in one behavior and
  the resolver checked in the next.
  """
  let _h: TestHelper
  embed _tracker: _TestALPNResolverTracker = _TestALPNResolverTracker
  var _ctx: (SSLContext val | None) = None

  new create(h: TestHelper) =>
    _h = h

  be run() =>
    // `resolver` is a local, so once this behavior returns the context is the
    // only thing that can be keeping it alive.
    let resolver = _tracker.resolver()

    try
      _ctx = _TestALPNContext(_h, resolver)?
    else
      _h.complete(false)
      return
    end

    @pony_triggergc(@pony_ctx())
    _check()

  be _check() =>
    if _tracker.collected() then
      _h.fail("the context did not keep the resolver alive")
      _h.complete(false)
      return
    end

    let ctx =
      match \exhaustive\ _ctx
      | let c: SSLContext val => c
      | None =>
        _h.fail("the context was never created")
        _h.complete(false)
        return
      end

    (let client, let server) =
      try
        _TestSSLSessionPair.from_context(_h, ctx)?
      else
        _h.complete(false)
        return
      end

    match \exhaustive\ client.alpn_selected()
    | let protocol: ALPNProtocolName =>
      _h.assert_eq[String]("h2", protocol)
    | None =>
      _h.fail("the client did not negotiate an ALPN protocol")
    end

    client.dispose()
    server.dispose()
    _h.complete(true)

actor \nodoc\ _TestALPNResolverSessionRooting
  """
  Pony collects between behaviors, so the sessions are made in one behavior and
  the resolver checked in the next.
  """
  let _h: TestHelper
  embed _tracker: _TestALPNResolverTracker = _TestALPNResolverTracker
  var _client: (SSL | None) = None
  var _server: (SSL | None) = None

  new create(h: TestHelper) =>
    _h = h

  be run() =>
    // The resolver and the context are locals, so once this behavior returns
    // the sessions are the only thing keeping the resolver alive, through the
    // context they hold.
    let resolver = _tracker.resolver()

    try
      let ctx = _TestALPNContext(_h, resolver)?
      _client = ctx.client()?
      _server = ctx.server()?
    else
      _h.fail("could not create an SSL session pair")
      _h.complete(false)
      return
    end

    @pony_triggergc(@pony_ctx())
    _check()

  be _check() =>
    (let client, let server) =
      try
        (_client as SSL, _server as SSL)
      else
        _h.fail("the sessions were never created")
        _h.complete(false)
        return
      end

    if _tracker.collected() then
      _h.fail("the sessions did not keep the resolver alive")
      client.dispose()
      server.dispose()
      _h.complete(false)
      return
    end

    try
      _TestSSLSessionPair._handshake(_h, client, server)?
    else
      client.dispose()
      server.dispose()
      _h.complete(false)
      return
    end

    match \exhaustive\ client.alpn_selected()
    | let protocol: ALPNProtocolName =>
      _h.assert_eq[String]("h2", protocol)
    | None =>
      _h.fail("the client did not negotiate an ALPN protocol")
    end

    client.dispose()
    server.dispose()
    _h.complete(true)

class \nodoc\ iso _TestSSLContextALPNResolverRootedByContext is UnitTest
  """
  A context keeps its ALPN resolver alive.

  `alpn_set_resolver` hands OpenSSL a raw pointer to the resolver, and the Pony
  garbage collector cannot see it. Without a reference on the Pony side, a
  collection frees the resolver while OpenSSL still calls it on every later
  server-side handshake.

  The resolver reports its own collection from `_final`. This test drops every
  reference to it but the context's, triggers a collection, and then negotiates
  ALPN through the resolver the context held.

  The negotiation runs only once the resolver is known to be alive. A resolver
  that was collected would be a use after free, and a crash tells nobody which
  test caused it.
  """
  fun name(): String =>
    "net/ssl/SSLContext.alpn_set_resolver/resolver_rooted_by_context"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _TestALPNResolverContextRooting(h).run()

class \nodoc\ iso _TestSSLContextALPNResolverRootedBySession is UnitTest
  """
  A session keeps the ALPN resolver alive after the context that made it is
  dropped.

  `SSL_new` takes a reference on the `SSL_CTX`, so the `SSL_CTX` outlives an
  `SSLContext` the caller drops while a session is still alive, and the ALPN
  select callback OpenSSL reads out of it still points at the resolver. A
  session holds the `SSLContext`, which holds the resolver, so both live as long
  as the session does. This test keeps only the sessions and drops the context
  the way a caller would.

  Carries the same caveats as
  `net/ssl/SSLContext.alpn_set_resolver/resolver_rooted_by_context`.
  """
  fun name(): String =>
    "net/ssl/SSLContext.alpn_set_resolver/resolver_rooted_by_session"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _TestALPNResolverSessionRooting(h).run()

actor \nodoc\ _TestALPNResolverUnreferenced
  """
  Pony collects between behaviors, so the context is built in one behavior and
  the resolver checked in the next.
  """
  let _h: TestHelper
  embed _tracker: _TestALPNResolverTracker = _TestALPNResolverTracker

  new create(h: TestHelper) =>
    _h = h

  be run() =>
    // The resolver, the context, and the session are all locals. Once this
    // behavior returns nothing roots the resolver.
    let resolver = _tracker.resolver()

    try
      _TestALPNContext(_h, resolver)?.client()?.dispose()
    else
      _h.fail("could not create an SSL session")
      _h.complete(false)
      return
    end

    @pony_triggergc(@pony_ctx())
    _check()

  be _check() =>
    _h.assert_true(
      _tracker.collected(),
      "a resolver nothing holds should be collected")
    _h.complete(true)

class \nodoc\ iso _TestSSLContextALPNResolverUnreferenced is UnitTest
  """
  A resolver that nothing holds is collected.

  This is the positive control for
  `net/ssl/SSLContext.alpn_set_resolver/resolver_rooted_by_context` and
  `..._by_session`, which pass by the resolver *not* being collected. It builds
  the same resolver, keeps neither the context nor a session, and shows the
  collection happens. Without it, a change that stopped collections altogether
  would leave both rooting tests passing for the wrong reason.
  """
  fun name(): String =>
    "net/ssl/SSLContext.alpn_set_resolver/resolver_collected_when_unreferenced"

  fun apply(h: TestHelper) =>
    h.long_test(2_000_000_000)
    _TestALPNResolverUnreferenced(h).run()

class \nodoc\ iso _TestSSLContextALPNSetClientProtocolsAfterDispose is UnitTest
  """
  `alpn_set_client_protocols` on a disposed context returns `false` rather than
  passing a null `SSL_CTX*` to `SSL_CTX_set_alpn_protos`, which dereferences it
  on every backend.

  The protocol list is valid and the live context accepts it, so the `false`
  afterwards is the disposed check and not the encoding failure that also
  returns `false`.
  """
  fun name(): String =>
    "net/ssl/SSLContext.alpn_set_client_protocols/after_dispose"

  fun apply(h: TestHelper) =>
    let ctx = SSLContext

    h.assert_true(
      ctx.alpn_set_client_protocols(["h2"]),
      "alpn_set_client_protocols() on a live context should return true")

    ctx.dispose()

    h.assert_false(
      ctx.alpn_set_client_protocols(["h2"]),
      "alpn_set_client_protocols() on a disposed context should return false")

class \nodoc\ iso _TestSSLContextSetMinProtoVersionInvertedRange is UnitTest
  """
  `set_min_proto_version` raises when the new minimum is above the current
  maximum, and does not change the minimum.

  An equal range is valid: it pins the context to one protocol version.
  `SSLAutoVersion` bypasses the check, since a zero boundary means the library
  picks.
  """
  fun name(): String =>
    "net/ssl/SSLContext.set_min_proto_version/inverted_range"

  fun apply(h: TestHelper) =>
    let ctx = SSLContext

    try
      ctx.set_max_proto_version(TLS1u2Version())?
    else
      h.fail("set_max_proto_version(TLS1u2Version) should not raise")
      return
    end

    try
      ctx.set_min_proto_version(TLS1u3Version())?
      h.fail(
        "set_min_proto_version(TLS1u3Version) should raise " +
          "when max is TLS 1.2")
    end

    h.assert_eq[ILong](
      TLS1u2Version().ilong(),
      ctx.get_min_proto_version(),
      "min should still be TLS 1.2 after the rejected call")

    try
      ctx.set_min_proto_version(SSLAutoVersion())?
    else
      h.fail("set_min_proto_version(SSLAutoVersion) should not raise")
    end

    try
      ctx.set_min_proto_version(TLS1u2Version())?
    else
      h.fail(
        "set_min_proto_version(TLS1u2Version) should not " +
          "raise when max is TLS 1.2")
    end

    ctx.dispose()

class \nodoc\ iso _TestSSLContextSetMaxProtoVersionInvertedRange is UnitTest
  """
  `set_max_proto_version` raises when the new maximum is below the current
  minimum, and does not change the maximum.

  An equal range is valid: it pins the context to one protocol version.
  `SSLAutoVersion` bypasses the check, since a zero boundary means the library
  picks.
  """
  fun name(): String =>
    "net/ssl/SSLContext.set_max_proto_version/inverted_range"

  fun apply(h: TestHelper) =>
    let ctx = SSLContext

    try
      ctx.set_min_proto_version(TLS1u3Version())?
    else
      h.fail("set_min_proto_version(TLS1u3Version) should not raise")
      return
    end

    try
      ctx.set_max_proto_version(TLS1u2Version())?
      h.fail(
        "set_max_proto_version(TLS1u2Version) should raise " +
          "when min is TLS 1.3")
    end

    h.assert_eq[ILong](
      SSLAutoVersion().ilong(),
      ctx.get_max_proto_version(),
      "max should still be auto after the rejected call")

    try
      ctx.set_max_proto_version(TLS1u3Version())?
    else
      h.fail(
        "set_max_proto_version(TLS1u3Version) should not " +
          "raise when min is TLS 1.3")
    end

    try
      ctx.set_max_proto_version(SSLAutoVersion())?
    else
      h.fail("set_max_proto_version(SSLAutoVersion) should not raise")
    end

    ctx.dispose()

class \nodoc\ iso _TestSSLContextSetMinProtoVersionAfterDispose is UnitTest
  """
  `set_min_proto_version` on a disposed context raises an error rather than
  passing a null `SSL_CTX*` to `SSL_CTX_ctrl`.

  This test passes with and without the disposed check on OpenSSL, whose
  `SSL_CTX_ctrl` returns 0 for a null context, which this method already turns
  into an error. LibreSSL's `SSL_CTX_ctrl` dereferences the context instead, so
  LibreSSL is where the check keeps this from being a crash.
  """
  fun name(): String => "net/ssl/SSLContext.set_min_proto_version/after_dispose"

  fun apply(h: TestHelper) =>
    let ctx = SSLContext

    try
      ctx.set_min_proto_version(TLS1u2Version())?
    else
      h.fail("set_min_proto_version() on a live context should not raise")
    end

    ctx.dispose()

    try
      ctx.set_min_proto_version(TLS1u2Version())?
      h.fail("set_min_proto_version() on a disposed context should raise")
    end

class \nodoc\ iso _TestSSLContextSetMaxProtoVersionAfterDispose is UnitTest
  """
  `set_max_proto_version` on a disposed context raises an error rather than
  passing a null `SSL_CTX*` to `SSL_CTX_ctrl`.

  Carries the same caveat as
  `net/ssl/SSLContext.set_min_proto_version/after_dispose`: only LibreSSL
  crashes without the check.
  """
  fun name(): String => "net/ssl/SSLContext.set_max_proto_version/after_dispose"

  fun apply(h: TestHelper) =>
    let ctx = SSLContext

    try
      ctx.set_max_proto_version(TLS1u3Version())?
    else
      h.fail("set_max_proto_version() on a live context should not raise")
    end

    ctx.dispose()

    try
      ctx.set_max_proto_version(TLS1u3Version())?
      h.fail("set_max_proto_version() on a disposed context should raise")
    end

class \nodoc\ iso _TestSSLContextGetMinProtoVersionAfterDispose is UnitTest
  """
  `get_min_proto_version` on a disposed context returns `SSLAutoVersion` rather
  than passing a null `SSL_CTX*` to `SSL_CTX_ctrl`.

  `create` sets the minimum to `TLS1u2Version`, so the `SSLAutoVersion`
  afterwards is not the value a live context would have returned.

  This test passes with and without the disposed check on OpenSSL, whose
  `SSL_CTX_ctrl` returns 0 for a null context. LibreSSL's dereferences it, so
  LibreSSL is where the check keeps this from being a crash.
  """
  fun name(): String => "net/ssl/SSLContext.get_min_proto_version/after_dispose"

  fun apply(h: TestHelper) =>
    let ctx = SSLContext

    h.assert_eq[ILong](
      TLS1u2Version().ilong(),
      ctx.get_min_proto_version(),
      "create() should have set the minimum protocol version to TLS v1.2")

    ctx.dispose()

    h.assert_eq[ILong](
      SSLAutoVersion().ilong(),
      ctx.get_min_proto_version(),
      "get_min_proto_version() on a disposed context should " +
        "return SSLAutoVersion")

class \nodoc\ iso _TestSSLContextGetMaxProtoVersionAfterDispose is UnitTest
  """
  `get_max_proto_version` on a disposed context returns `SSLAutoVersion` rather
  than passing a null `SSL_CTX*` to `SSL_CTX_ctrl`.

  `create` leaves the maximum at `SSLAutoVersion`, so this test sets it to
  `TLS1u3Version` first. Without that, the assertion after the dispose would
  hold for a live context too.

  Carries the same caveat as
  `net/ssl/SSLContext.get_min_proto_version/after_dispose`: only LibreSSL
  crashes without the check.
  """
  fun name(): String => "net/ssl/SSLContext.get_max_proto_version/after_dispose"

  fun apply(h: TestHelper) =>
    let ctx = SSLContext

    try
      ctx.set_max_proto_version(TLS1u3Version())?
    else
      h.fail("set_max_proto_version() on a live context should not raise")
      return
    end

    h.assert_eq[ILong](
      TLS1u3Version().ilong(),
      ctx.get_max_proto_version(),
      "the maximum protocol version should read back as TLS v1.3")

    ctx.dispose()

    h.assert_eq[ILong](
      SSLAutoVersion().ilong(),
      ctx.get_max_proto_version(),
      "get_max_proto_version() on a disposed context should " +
        "return SSLAutoVersion")

class \nodoc\ iso _TestSSLContextGetMinProtoVersionOnValReceiver is UnitTest
  """
  `get_min_proto_version` reads through a `val` receiver.

  Configuring a context and then holding it `val` is what `client` and `server`
  require. A `fun ref` getter cannot be called on a `val` receiver, so this file
  stops compiling if that capability comes back.

  `create` sets the minimum to `TLS1u2Version`, so the assertion is on a value
  the getter had to read out of the context rather than on a default.
  """
  fun name(): String =>
    "net/ssl/SSLContext.get_min_proto_version/on_val_receiver"

  fun apply(h: TestHelper) =>
    let ctx: SSLContext val = recover val SSLContext end

    h.assert_eq[ILong](
      TLS1u2Version().ilong(),
      ctx.get_min_proto_version(),
      "a val receiver should read back the minimum that create() set")

class \nodoc\ iso _TestSSLContextGetMaxProtoVersionOnValReceiver is UnitTest
  """
  `get_max_proto_version` reads through a `val` receiver.

  Configuring a context and then holding it `val` is what `client` and `server`
  require. A `fun ref` getter cannot be called on a `val` receiver, so this file
  stops compiling if that capability comes back.

  `create` leaves the maximum at `SSLAutoVersion`, so this context sets it to
  `TLS1u3Version` before it freezes. Without that, the assertion would hold
  for a context whose maximum was never set.
  """
  fun name(): String =>
    "net/ssl/SSLContext.get_max_proto_version/on_val_receiver"

  fun apply(h: TestHelper) ? =>
    let ctx: SSLContext val =
      recover val
        SSLContext .> set_max_proto_version(TLS1u3Version())?
      end

    h.assert_eq[ILong](
      TLS1u3Version().ilong(),
      ctx.get_max_proto_version(),
      "a val receiver should read back the maximum that was set")

class \nodoc\ iso _TestSSLContextSetAuthorityRootCertsAfterDispose is UnitTest
  """
  `set_authority(None, None)` on a disposed context raises an error rather than
  loading the system root certificates into a null `SSL_CTX*`.

  On Posix this raises whether or not the context is disposed; there are no
  system root certificates to load and the method has always raised. On Windows
  a live context loads them, and a disposed one used to reach
  `SSL_CTX_set_cert_store` with a null context and crash. Windows CI is where
  this test earns its keep.
  """
  fun name(): String =>
    "net/ssl/SSLContext.set_authority/root_certs_after_dispose"

  fun apply(h: TestHelper) =>
    let ctx = SSLContext

    ctx.dispose()

    try
      ctx.set_authority(None, None)?
      h.fail("set_authority(None, None) on a disposed context should raise")
    end

class \nodoc\ iso _TestSSLContextSetAuthorityAfterDispose is UnitTest
  """
  `set_authority` on a disposed context raises an error. This locks in a guard
  the disposed context already had.
  """
  fun name(): String => "net/ssl/SSLContext.set_authority/after_dispose"

  fun apply(h: TestHelper) =>
    let auth = FileAuth(h.env.root)
    let ctx = SSLContext

    try
      ctx.set_authority(FilePath(auth, "assets/cert.pem"))?
    else
      h.fail("set_authority() on a live context should not raise")
    end

    ctx.dispose()

    try
      ctx.set_authority(FilePath(auth, "assets/cert.pem"))?
      h.fail("set_authority() on a disposed context should raise")
    end

class \nodoc\ iso _TestSSLContextSetCertAfterDispose is UnitTest
  """
  `set_cert` on a disposed context raises an error. This locks in a guard the
  disposed context already had.
  """
  fun name(): String => "net/ssl/SSLContext.set_cert/after_dispose"

  fun apply(h: TestHelper) =>
    let auth = FileAuth(h.env.root)
    let cert = FilePath(auth, "assets/cert.pem")
    let key = FilePath(auth, "assets/key.pem")
    let ctx = SSLContext

    try
      ctx.set_cert(cert, key)?
    else
      h.fail("set_cert() on a live context should not raise")
    end

    ctx.dispose()

    try
      ctx.set_cert(cert, key)?
      h.fail("set_cert() on a disposed context should raise")
    end

class \nodoc\ iso _TestSSLContextSetCiphersAfterDispose is UnitTest
  """
  `set_ciphers` on a disposed context raises an error. This locks in a guard the
  disposed context already had.

  The cipher list is one a live context accepts, so the error after the dispose
  is the disposed check and not the invalid-cipher-list error.
  """
  fun name(): String => "net/ssl/SSLContext.set_ciphers/after_dispose"

  fun apply(h: TestHelper) =>
    let ctx = SSLContext

    try
      ctx.set_ciphers("HIGH")?
    else
      h.fail("set_ciphers(\"HIGH\") on a live context should not raise")
    end

    ctx.dispose()

    try
      ctx.set_ciphers("HIGH")?
      h.fail("set_ciphers() on a disposed context should raise")
    end

class \nodoc\ iso _TestSSLContextSetVerifyDepthAfterDispose is UnitTest
  """
  `set_verify_depth` on a disposed context does nothing. This locks in a guard
  the disposed context already had; without it, `SSL_CTX_set_verify_depth`
  dereferences the null context on every backend.

  There is nothing to observe beyond the call returning, so the context is
  checked for inertness afterwards. The call before the dispose is the only one
  in the suite that carries a depth into `SSL_CTX_set_verify_depth`.
  """
  fun name(): String => "net/ssl/SSLContext.set_verify_depth/after_dispose"

  fun apply(h: TestHelper) =>
    let ctx = SSLContext

    ctx.set_verify_depth(4)

    ctx.dispose()
    ctx.set_verify_depth(4)

    // A live context accepts this protocol list, so a `false` here is the
    // context still being disposed and not a list it rejected.
    h.assert_false(
      ctx.alpn_set_client_protocols(["h2"]),
      "the context should still be disposed")

class \nodoc\ iso _TestSSLContextAllowTLSAfterDispose is UnitTest
  """
  `allow_tls_v1`, `allow_tls_v1_1` and `allow_tls_v1_2` on a disposed context do
  nothing. This locks in guards the disposed context already had.

  Unlike the protocol version methods, these catch a missing guard on every
  backend. They reach `SSL_CTX_set_options` and `SSL_CTX_clear_options` on
  OpenSSL and `SSL_CTX_ctrl` on LibreSSL, and all three dereference the null
  context.

  Both states of each are exercised, because one clears an option and the other
  sets it, and they reach different C functions on OpenSSL.
  """
  fun name(): String => "net/ssl/SSLContext.allow_tls/after_dispose"

  fun apply(h: TestHelper) =>
    let ctx = SSLContext

    ctx.dispose()

    ctx.allow_tls_v1(true)
    ctx.allow_tls_v1(false)
    ctx.allow_tls_v1_1(true)
    ctx.allow_tls_v1_1(false)
    ctx.allow_tls_v1_2(true)
    ctx.allow_tls_v1_2(false)

    // A live context accepts this protocol list, so a `false` here is the
    // context still being disposed and not a list it rejected.
    h.assert_false(
      ctx.alpn_set_client_protocols(["h2"]),
      "the context should still be disposed")

class \nodoc\ iso _TestSSLContextAllowTLSV1u2 is UnitTest
  """
  `allow_tls_v1_2` takes effect on a live context.

  The context permits TLS 1.2 and nothing else, so disabling TLS 1.2 leaves the
  handshake no version to negotiate. Re-enabling it clears the option and the
  handshake completes again.

  The first assertion is the control. Without it, a handshake that failed for
  an unrelated reason would look like the option taking effect.

  The context is live, so `SSLContext._set_options` and `_clear_options` reach
  the SSL library rather than returning at the disposed check.
  """
  fun name(): String => "net/ssl/SSLContext.allow_tls_v1_2"

  fun apply(h: TestHelper) =>
    h.assert_true(
      _handshakes(h, false, false),
      "a context pinned to TLS 1.2 should complete a handshake")

    h.assert_false(
      _handshakes(h, true, false),
      "disabling TLS 1.2 should leave no version to negotiate")

    h.assert_true(
      _handshakes(h, true, true),
      "re-enabling TLS 1.2 should let the handshake complete again")

  fun _handshakes(h: TestHelper, disable: Bool, reenable: Bool): Bool =>
    """
    Whether a client and a server session from a TLS 1.2 only context complete
    a handshake, having disabled and then re-enabled TLS 1.2 as asked.
    """
    let auth = FileAuth(h.env.root)
    let sslctx =
      try
        recover val
          let ctx = SSLContext
            .> set_authority(FilePath(auth, "assets/cert.pem"))?
            .> set_cert(
                FilePath(auth, "assets/cert.pem"),
                FilePath(auth, "assets/key.pem"))?
            .> set_client_verify(false)
            .> set_server_verify(false)
            .> set_min_proto_version(TLS1u2Version())?
            .> set_max_proto_version(TLS1u2Version())?
          if disable then ctx.allow_tls_v1_2(false) end
          if reenable then ctx.allow_tls_v1_2(true) end
          ctx
        end
      else
        h.fail("ssl context setup failed")
        return false
      end

    let client: SSL =
      try
        sslctx.client()?
      else
        h.fail("failed getting ssl client session")
        return false
      end

    let server: SSL =
      try
        sslctx.server()?
      else
        client.dispose()
        h.fail("failed getting ssl server session")
        return false
      end

    let ready = _drive(client, server)
    client.dispose()
    server.dispose()
    ready

  fun _drive(client: SSL, server: SSL): Bool =>
    """
    Whether both sides reach `SSLReady` when each side's outgoing bytes are
    handed straight to the other.

    A handshake with no protocol version left to negotiate fails rather than
    stalls, so the loop ends on its own. The round cap is a backstop against a
    session that never settles, not the expected way out.
    """
    let max_rounds: USize = 20
    var rounds: USize = 0
    var cr: SSLReceiveResult = SSLAccepted
    var sr: SSLReceiveResult = SSLAccepted

    while
      (cr is SSLAccepted) or (sr is SSLAccepted)
    do
      if rounds == max_rounds then return false end
      rounds = rounds + 1

      let sr' = _TestSSLTransfer(client, server)
      let cr' = _TestSSLTransfer(server, client)
      if not (sr' is SSLAccepted) then sr = sr' end
      if not (cr' is SSLAccepted) then cr = cr' end
    end

    (cr is SSLReady) and (sr is SSLReady)

class \nodoc\ iso _TestSSLContextClientAfterDispose is UnitTest
  """
  `client` on a disposed context raises an error rather than handing a null
  context to `SSL_new`.

  Two things make it raise and the test cannot tell them apart: `SSL._create`
  checks the context before it calls `SSL_new`, and `SSL_new` returns null for a
  null context on every backend, which `SSL._create` raises on as well.
  """
  fun name(): String => "net/ssl/SSLContext.client/after_dispose"

  fun apply(h: TestHelper) =>
    let live: SSLContext val = recover val SSLContext end

    try
      live.client()?.dispose()
    else
      h.fail("client() on a live context should not raise")
    end

    // `client` needs an immutable context, so dispose the mutable one first and
    // then freeze it to call `client` on the disposed result.
    let mutable: SSLContext iso = recover iso SSLContext end
    mutable.dispose()
    let disposed: SSLContext val = consume mutable

    try
      disposed.client()?.dispose()
      h.fail("client() on a disposed context should raise")
    end

class \nodoc\ iso _TestSSLContextServerAfterDispose is UnitTest
  """
  `server` on a disposed context raises an error rather than handing a null
  context to `SSL_new`.

  Carries the same caveat as `net/ssl/SSLContext.client/after_dispose`: the
  context check in `SSL._create` and `SSL_new` returning null both produce the
  error, and the test cannot tell them apart.
  """
  fun name(): String => "net/ssl/SSLContext.server/after_dispose"

  fun apply(h: TestHelper) =>
    let live: SSLContext val = recover val SSLContext end

    try
      live.server()?.dispose()
    else
      h.fail("server() on a live context should not raise")
    end

    // `server` needs an immutable context, so dispose the mutable one first and
    // then freeze it to call `server` on the disposed result.
    let mutable: SSLContext iso = recover iso SSLContext end
    mutable.dispose()
    let disposed: SSLContext val = consume mutable

    try
      disposed.server()?.dispose()
      h.fail("server() on a disposed context should raise")
    end

class \nodoc\ iso _TestALPNProtocolListRoundTrip is Property1[Array[String]]
  fun name(): String =>
    "net/ssl/_ALPNProtocolList/property/roundtrip"

  fun gen(): Generator[Array[String]] =>
    Generators.array_of[String](
      Generators.ascii_printable(1, 20) where from = 1, to = 5)

  fun ref property(sample: Array[String], h: PropertyHelper) ? =>
    let encoded = _ALPNProtocolList.from_array(sample)?
    let decoded = _ALPNProtocolList.to_array(encoded)?
    h.assert_eq[USize](sample.size(), decoded.size())
    var i: USize = 0
    while i < sample.size() do
      h.assert_true(sample(i)? == decoded(i)?)
      i = i + 1
    end

class \nodoc\ iso _TestSSLCloseFromReady is UnitTest
  """
  Calling `close` on a ready session sets the state to `SSLClosed` and queues
  a `close_notify` alert in the output BIO.
  """
  fun name(): String => "net/ssl/SSL.close/from_ready"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    h.assert_true(
      client.send() is None,
      "nothing queued before close")

    client.close()

    h.assert_true(
      client.send() isnt None,
      "close_notify should be queued in the output BIO")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLCloseIdempotent is UnitTest
  """
  A second `close` call is a no-op — it produces no additional output.
  """
  fun name(): String => "net/ssl/SSL.close/idempotent"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    client.close()

    while true do
      match \exhaustive\ client.send()
      | let _: Array[U8] iso => None
      | None => break
      end
    end

    client.close()

    h.assert_true(
      client.send() is None,
      "second close should produce no additional output")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLCloseFromWrongStates is UnitTest
  """
  `close` is a no-op during handshake, after auth failure, and after error.
  """
  fun name(): String => "net/ssl/SSL.close/from_wrong_states"

  fun apply(h: TestHelper) =>
    // Handshaking: a fresh client before any handshake bytes are exchanged
    (let client, let server) =
      try
        _TestSSLSessionPair.fresh(h)?
      else
        h.fail("could not create fresh sessions")
        return
      end

    client.close()

    h.assert_true(
      client.receive("") is SSLAccepted,
      "close during handshake should leave session still handshaking")

    // SSLAuthFail: a verifying client whose peer presents an untrusted chain
    (let auth_client, let auth_server, let acr, _) =
      try
        _TestSSLSessionPair.attempt(
          h,
          _TestSSLContext(h where client_verify = true)?,
          _TestSSLContext(h where cert = true)?)?
      else
        h.fail("could not create an auth-fail session pair")
        return
      end

    h.assert_true(
      acr is SSLAuthFail,
      "client should report SSLAuthFail")

    auth_client.close()

    h.assert_true(
      auth_client.receive("") is SSLAuthFail,
      "receive should still report SSLAuthFail after close")

    auth_client.dispose()
    auth_server.dispose()

    // SSLError: feed garbage to a fresh client to break its handshake
    let er = client.receive("NOT A TLS RECORD")

    h.assert_true(
      er is SSLError,
      "client should report SSLError after garbage")

    client.close()

    h.assert_true(
      client.receive("") is SSLError,
      "receive should still report SSLError after close")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLCloseAfterDispose is UnitTest
  """
  `close` after `dispose` is a no-op.
  """
  fun name(): String => "net/ssl/SSL.close/after_dispose"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    client.dispose()
    client.close()

    h.assert_true(
      client.send() is None,
      "close after dispose should not queue anything")

    server.dispose()

class \nodoc\ iso _TestSSLPeerCloseNotifyDetected is UnitTest
  """
  When one session sends `close_notify`, the other detects it via `read` and
  transitions to `SSLClosed`, not `SSLError`.
  """
  fun name(): String => "net/ssl/SSL.read/peer_close_notify"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    client.close()

    _TestSSLTransfer(client, server)

    match \exhaustive\ server.read()
    | let _: Array[U8] iso =>
      h.fail("server read produced data from a close_notify")
    | None =>
      h.fail("server should report SSLClosed, not None")
    | SSLClosed => None
    | SSLError =>
      h.fail("server should report SSLClosed, not SSLError")
    | InvalidOperation =>
      h.fail("server should report SSLClosed, not InvalidOperation")
    end

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLBufferedDataDrainedOnCloseNotify is UnitTest
  """
  When `expect`-mode has accumulated partial data and the peer sends
  `close_notify`, the buffered data is returned before the state transitions
  to `SSLClosed`.
  """
  fun name(): String => "net/ssl/SSL.read/buffered_drain_on_close_notify"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    // Send 50 bytes, read with expect=100 to accumulate in _read_buf
    try
      client.write(recover val Array[U8].init('A', 50) end)?
      _TestSSLTransfer(client, server)
    else
      h.fail("could not send application data")
      client.dispose()
      server.dispose()
      return
    end

    match \exhaustive\ server.read(100)
    | let _: Array[U8] iso =>
      h.fail("50 bytes should not satisfy a read of 100")
      client.dispose()
      server.dispose()
      return
    | None => None
    | SSLClosed =>
      h.fail("read returned SSLClosed before any close")
      client.dispose()
      server.dispose()
      return
    | SSLError =>
      h.fail("read returned SSLError on a healthy session")
      client.dispose()
      server.dispose()
      return
    | InvalidOperation =>
      h.fail("read returned InvalidOperation on a healthy session")
      client.dispose()
      server.dispose()
      return
    end

    // Now client sends close_notify
    client.close()

    _TestSSLTransfer(client, server)

    // The read should drain the 50 buffered bytes before transitioning
    match \exhaustive\ server.read(100)
    | let data: Array[U8] iso =>
      h.assert_eq[USize](
        50,
        (consume data).size(),
        "buffered data should be drained on close_notify")
    | None =>
      h.fail("read should have returned the 50 buffered bytes")
    | SSLClosed => None
    | SSLError =>
      h.fail("read returned SSLError instead of buffered data")
    | InvalidOperation =>
      h.fail("read returned InvalidOperation instead of buffered data")
    end

    // After draining, next read should report SSLClosed
    match \exhaustive\ server.read()
    | let _: Array[U8] iso =>
      h.fail("second read produced data after draining")
    | None =>
      h.fail("second read returned None instead of SSLClosed")
    | SSLClosed => None
    | SSLError =>
      h.fail("second read returned SSLError instead of SSLClosed")
    | InvalidOperation =>
      h.fail("second read returned InvalidOperation instead of SSLClosed")
    end

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLReceiveInClosed is UnitTest
  """
  `receive` keeps working in `SSLClosed` — data fed into the BIO after
  `close` is not silently dropped, and `read` can still decrypt it.
  """
  fun name(): String => "net/ssl/SSL.receive/in_closed"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    // Server encrypts data before client closes
    try
      server.write("hello")?
    else
      h.fail("server could not encrypt application data")
      client.dispose()
      server.dispose()
      return
    end

    client.close()

    // Transfer the encrypted bytes to the client after close — this calls
    // client.receive() in SSLClosed state
    _TestSSLTransfer(server, client)

    match \exhaustive\ client.read()
    | let data: Array[U8] iso =>
      h.assert_eq[String]("hello", String.from_array(consume data))
    | None =>
      h.fail("read should return data received after close")
    | SSLClosed => h.fail("read returned SSLClosed instead of data")
    | SSLError => h.fail("read returned SSLError instead of data")
    | InvalidOperation =>
      h.fail("read returned InvalidOperation instead of data")
    end

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLWriteBlockedInClosed is UnitTest
  """
  `write` raises an error in `SSLClosed`.
  """
  fun name(): String => "net/ssl/SSL.write/blocked_in_closed"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    client.close()

    let write_succeeded =
      try
        client.write("should fail")?
        true
      else
        false
      end

    h.assert_false(
      write_succeeded,
      "write should raise an error in SSLClosed")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLBidirectionalCloseRoundtrip is UnitTest
  """
  Full bidirectional shutdown: one side calls `close`, the other detects it
  via `read` and calls `close` in response. Both sides end up in `SSLClosed`
  with each other's `close_notify` delivered.
  """
  fun name(): String => "net/ssl/SSL.close/bidirectional_roundtrip"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    // Client initiates shutdown
    client.close()

    _TestSSLTransfer(client, server)

    // Server reads and detects close_notify
    let sr = server.read()

    h.assert_true(
      sr is SSLClosed,
      "server should report SSLClosed after receiving client's close_notify")

    // Server responds with its own close_notify
    server.close()

    _TestSSLTransfer(server, client)

    // Client reads server's close_notify response
    let cr = client.read()

    h.assert_true(
      cr is SSLClosed,
      "client should report SSLClosed")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLCorruptRecordInClosed is UnitTest
  """
  A corrupted record received while in `_SSLClosed` transitions to `SSLError`.

  The session has already sent its own `close_notify` and is in `_SSLClosed`.
  A corrupted record arriving now is a genuine error. `_SSLClosed.read`
  preserves `_SSLClosed` on a clean `zero_return` but lets `_Errored`
  through.
  """
  fun name(): String => "net/ssl/SSL.read/corrupt_record_in_closed"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    // Encrypt a record from the server before the close flow starts.
    // This record belongs to the same TLS session, so corrupting it triggers
    // a MAC failure the client's session can detect.
    var saved_record: Array[U8] iso = recover iso Array[U8] end
    try
      server.write("payload")?
      saved_record =
        match \exhaustive\ server.send()
        | let d: Array[U8] iso => consume d
        | None => error
        end
    else
      h.fail("could not produce a record to corrupt")
      client.dispose()
      server.dispose()
      return
    end

    // Get the client into _SSLClosed by calling close()
    client.close()

    // Feed the corrupted record — same session, so the MAC failure is real
    try
      let last = saved_record.size() - 1
      saved_record(last)? = saved_record(last)? xor 0xFF
      client.receive(consume saved_record)
    else
      h.fail("could not corrupt the saved record")
      client.dispose()
      server.dispose()
      return
    end

    let cr = client.read()

    h.assert_true(
      cr is SSLError,
      "a corrupted record in _SSLClosed should report SSLError from read")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLSendOnFailedSession is UnitTest
  """
  `send` works on a failed session, allowing retrieval of alert bytes that
  OpenSSL queued in the output BIO before the failure.

  A handshake failure from non-TLS input produces an alert. `send` returns
  those bytes, and a subsequent `send` returns `None`.
  """
  fun name(): String => "net/ssl/SSL.send/on_failed_session"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair.fresh(h)?
      else
        h.fail("could not create a fresh SSL session pair")
        return
      end

    // Drive the handshake far enough that the server has state, then corrupt it
    _TestSSLTransfer(client, server)
    _TestSSLTransfer(server, client)

    // Feed non-TLS bytes to the server to trigger an error with an alert
    let sr = server.receive("NOT TLS DATA\r\n")

    h.assert_true(
      sr is SSLError,
      "server should report SSLError after receiving non-TLS bytes")

    match \exhaustive\ server.send()
    | let alert: Array[U8] iso =>
      h.assert_true(
        (consume alert).size() > 0,
        "alert bytes from a failed session should not be empty")
    | None =>
      h.fail("failed session should have alert bytes to send")
    end

    h.assert_true(
      server.send() is None,
      "send should return None after sending the alert")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestSSLWriteBlockedInClosing is UnitTest
  """
  `write` raises an error in `_SSLClosing`.

  The peer has sent `close_notify`, and we have not responded yet. Writing
  application data is not allowed — the only valid next step is `close` to
  send our own `close_notify`.
  """
  fun name(): String => "net/ssl/SSL.write/blocked_in_closing"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestSSLSessionPair(h)?
      else
        h.fail("could not establish an SSL session pair")
        return
      end

    // Get server into _SSLClosing: client sends close_notify, server reads it
    client.close()

    _TestSSLTransfer(client, server)

    let sr = server.read()

    h.assert_true(
      sr is SSLClosed,
      "server should report SSLClosed after receiving peer's close_notify")

    let write_succeeded =
      try
        server.write("should fail")?
        true
      else
        false
      end

    h.assert_false(
      write_succeeded,
      "write should raise an error in _SSLClosing")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestMatchNameEmptyName is UnitTest
  fun name(): String => "net/ssl/X509._match_name/empty_name"

  fun apply(h: TestHelper) =>
    h.assert_false(X509._match_name("example.com", ""))
    h.assert_false(X509._match_name("localhost", ""))
    h.assert_false(X509._match_name("192.168.1.1", ""))

class \nodoc\ iso _TestMatchNameExactCaseInsensitive is UnitTest
  fun name(): String => "net/ssl/X509._match_name/exact_case_insensitive"

  fun apply(h: TestHelper) =>
    h.assert_true(X509._match_name("example.com", "example.com"))
    h.assert_true(X509._match_name("Example.COM", "example.com"))
    h.assert_true(X509._match_name("example.com", "EXAMPLE.COM"))

class \nodoc\ iso _TestMatchNameNoMatch is UnitTest
  fun name(): String => "net/ssl/X509._match_name/no_match"

  fun apply(h: TestHelper) =>
    h.assert_false(X509._match_name("example.com", "other.com"))
    h.assert_false(X509._match_name("example.com", "example.org"))

class \nodoc\ iso _TestMatchNameIPLiteral is UnitTest
  fun name(): String => "net/ssl/X509._match_name/ip_literal"

  fun apply(h: TestHelper) =>
    h.assert_true(X509._match_name("192.168.1.1", "192.168.1.1"))
    h.assert_false(X509._match_name("192.168.1.1", "192.168.1.2"))
    // IP literals require exact match, not case-insensitive domain match
    h.assert_false(X509._match_name("192.168.1.1", "*.168.1.1"))

class \nodoc\ iso _TestMatchNameWildcard is UnitTest
  fun name(): String => "net/ssl/X509._match_name/wildcard"

  fun apply(h: TestHelper) =>
    h.assert_true(X509._match_name("foo.example.com", "*.example.com"))
    h.assert_true(X509._match_name("FOO.Example.COM", "*.example.com"))

class \nodoc\ iso _TestMatchNameWildcardInsufficientLevels is UnitTest
  fun name(): String =>
    "net/ssl/X509._match_name/wildcard_insufficient_levels"

  fun apply(h: TestHelper) =>
    // Wildcard alone is not valid
    h.assert_false(X509._match_name("example.com", "*"))
    // Wildcard with only one domain level is not valid
    h.assert_false(X509._match_name("example.com", "*."))
    // Wildcard followed by dot-dot is not valid
    h.assert_false(X509._match_name("foo.example.com", "*..com"))

primitive \nodoc\ _TestDTLSContext
  fun val apply(
    h: TestHelper,
    cert: Bool = false,
    authority: Bool = false,
    client_verify: Bool = true,
    server_verify: Bool = false)
    : DTLSContext val ?
  =>
    let auth = FileAuth(h.env.root)

    try
      recover val
        let ctx: DTLSContext ref = DTLSContext
        if cert then
          ctx.set_cert(
            FilePath(auth, "assets/cert.pem"),
            FilePath(auth, "assets/key.pem"))?
        end
        if authority then
          ctx.set_authority(FilePath(auth, "assets/cert.pem"))?
        end
        ctx.set_client_verify(client_verify)
        ctx.set_server_verify(server_verify)
        ctx
      end
    else
      h.fail("dtls context setup failed")
      error
    end

primitive \nodoc\ _TestDTLSDefaultSessions
  fun val apply(h: TestHelper): (DTLS iso^, DTLS iso^) ? =>
    let dtlsctx =
      _TestDTLSContext(
        h
        where cert = true,
          authority = true,
          client_verify = false,
          server_verify = false)?

    let dtls_client =
      try
        dtlsctx.client()?
      else
        h.fail("failed getting dtls client session")
        error
      end
    let dtls_server =
      try
        dtlsctx.server()?
      else
        h.fail("failed getting dtls server session")
        error
      end

    (consume dtls_client, consume dtls_server)

primitive \nodoc\ _TestDTLSTransfer
  fun val apply(sender: DTLS, receiver: DTLS): SSLReceiveResult =>
    var result: SSLReceiveResult = SSLAccepted
    while true do
      match \exhaustive\ sender.send()
      | let data: Array[U8] iso =>
        result = receiver.receive(consume data)
      | None => break
      end
    end
    result

primitive \nodoc\ _TestDTLSSessionPair
  fun val apply(h: TestHelper): (DTLS, DTLS) ? =>
    (let client, let server) = fresh(h)?
    _handshake(h, client, server)?
    (client, server)

  fun val fresh(h: TestHelper): (DTLS, DTLS) ? =>
    (let client_session, let server_session) = _TestDTLSDefaultSessions(h)?
    let client: DTLS = consume client_session
    let server: DTLS = consume server_session
    (client, server)

  fun val attempt(
    h: TestHelper,
    client_ctx: DTLSContext val,
    server_ctx: DTLSContext val,
    hostname: String = "")
    : (DTLS, DTLS, SSLReceiveResult, SSLReceiveResult) ?
  =>
    let client: DTLS =
      try
        client_ctx.client(hostname)?
      else
        h.fail("failed getting dtls client session")
        error
      end
    let server: DTLS =
      try
        server_ctx.server()?
      else
        h.fail("failed getting dtls server session")
        client.dispose()
        error
      end
    (let cr, let sr) = _pump(client, server)
    (client, server, cr, sr)

  fun val _pump(
    client: DTLS,
    server: DTLS)
    : (SSLReceiveResult, SSLReceiveResult)
  =>
    var rounds: USize = 0
    var client_result: SSLReceiveResult = SSLAccepted
    var server_result: SSLReceiveResult = SSLAccepted

    while
      ((client_result is SSLAccepted) or
        (server_result is SSLAccepted)) and
        (rounds < _max_rounds())
    do
      rounds = rounds + 1
      let sr = _TestDTLSTransfer(client, server)
      let cr = _TestDTLSTransfer(server, client)
      if not (sr is SSLAccepted) then server_result = sr end
      if not (cr is SSLAccepted) then client_result = cr end
    end

    (client_result, server_result)

  fun val _max_rounds(): USize => 20

  fun val _handshake(h: TestHelper, client: DTLS, server: DTLS) ? =>
    (let cr, let sr) = _pump(client, server)

    if (cr is SSLAccepted) or (sr is SSLAccepted) then
      h.fail("in memory DTLS handshake did not finish")
      error
    end

    if (cr isnt SSLReady) or (sr isnt SSLReady) then
      h.fail("in memory DTLS handshake did not reach SSLReady")
      error
    end

class \nodoc\ iso _TestDTLSHandshakeInMemory is UnitTest
  """
  Two DTLS sessions can complete a handshake with no transport between them by
  handing each side's outgoing bytes straight to the other, and application
  data written by one can be read by the other.
  """
  fun name(): String => "net/dtls/DTLS/handshake_in_memory"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestDTLSSessionPair(h)?
      else
        h.fail("could not establish a DTLS session pair")
        return
      end

    try
      client.write("hello")?
      _TestDTLSTransfer(client, server)
    else
      h.fail("client could not send application data")
      client.dispose()
      server.dispose()
      return
    end

    match \exhaustive\ server.read()
    | let data: Array[U8] iso =>
      h.assert_eq[String]("hello", String.from_array(consume data))
    | None => h.fail("server read no application data")
    | SSLClosed => h.fail("server read returned SSLClosed")
    | SSLError => h.fail("server read returned SSLError")
    | InvalidOperation => h.fail("server read returned InvalidOperation")
    end

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestDTLSReceiveUntrustedChain is UnitTest
  """
  A verifying DTLS session whose peer presents a chain it does not trust
  reports `SSLAuthFail`.
  """
  fun name(): String => "net/dtls/DTLS.receive/untrusted_chain"

  fun apply(h: TestHelper) =>
    (let client, let server, let cr, _) =
      try
        _TestDTLSSessionPair.attempt(
          h,
          _TestDTLSContext(h where client_verify = true)?,
          _TestDTLSContext(h where cert = true)?)?
      else
        h.fail("could not create a DTLS session pair")
        return
      end

    h.assert_true(
      cr is SSLAuthFail,
      "a chain the client does not trust should report SSLAuthFail")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestDTLSReceiveVerifyOffNeverAuthFails is UnitTest
  """
  A DTLS session created with verification off reports `SSLError` when its
  handshake fails, not `SSLAuthFail`.
  """
  fun name(): String => "net/dtls/DTLS.receive/verify_off_never_auth_fails"

  fun apply(h: TestHelper) =>
    (let client, let server, let cr, _) =
      try
        _TestDTLSSessionPair.attempt(
          h,
          _TestDTLSContext(h where client_verify = false)?,
          _TestDTLSContext(
            h where cert = true, authority = true, server_verify = true)?)?
      else
        h.fail("could not create a DTLS session pair")
        return
      end

    h.assert_true(
      (cr is SSLError) or (cr is SSLReady),
      "a session that was not asked to verify should not report SSLAuthFail")

    h.assert_false(
      cr is SSLAuthFail,
      "a session that was not asked to verify should never report SSLAuthFail")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestDTLSReceiveHostnameMismatch is UnitTest
  """
  A verifying DTLS session whose peer presents a certificate that is not valid
  for the hostname reports `SSLAuthFail`.
  """
  fun name(): String => "net/dtls/DTLS.receive/hostname_mismatch"

  fun apply(h: TestHelper) =>
    (let client, let server, let cr, _) =
      try
        let client_ctx =
          _TestDTLSContext(h where authority = true, client_verify = true)?
        let server_ctx = _TestDTLSContext(h where cert = true)?
        _TestDTLSSessionPair.attempt(
          h, client_ctx, server_ctx where hostname = "nomatch.example.com")?
      else
        h.fail("could not create a DTLS session pair")
        return
      end

    h.assert_true(
      cr is SSLAuthFail,
      "a certificate not valid for the hostname should report SSLAuthFail")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestDTLSDisposeBeforeHandshake is UnitTest
  """
  A session disposed before its handshake finishes is inert: `read` returns
  `InvalidOperation`, `send` returns `None`, and `receive` returns
  `InvalidOperation`.

  A fresh client session has a ClientHello waiting to go out, so `send`
  returning `None` after the dispose is the disposed check and not an empty
  BIO.
  """
  fun name(): String => "net/dtls/DTLS.dispose/before_handshake"

  fun apply(h: TestHelper) =>
    let dtlsctx =
      try
        _TestDTLSContext(
          h
          where cert = true,
            authority = true,
            client_verify = false,
            server_verify = false)?
      else
        return
      end

    let client =
      try
        dtlsctx.client()?
      else
        h.fail("failed getting dtls client session")
        return
      end

    h.assert_true(
      client.send() isnt None,
      "a fresh client session should have a ClientHello to send")

    client.dispose()

    h.assert_true(
      client.read() is InvalidOperation,
      "read() on a disposed session should return InvalidOperation")
    h.assert_true(
      client.send() is None,
      "send() on a disposed session should return None")
    h.assert_true(
      client.receive("bytes that will never be decrypted") is InvalidOperation,
      "receive on a disposed session should return InvalidOperation")

class \nodoc\ iso _TestDTLSDisposeTwice is UnitTest
  """
  Disposing a DTLS session twice does not crash.
  """
  fun name(): String => "net/dtls/DTLS.dispose/twice"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestDTLSSessionPair(h)?
      else
        h.fail("could not establish a DTLS session pair")
        return
      end

    client.dispose()
    client.dispose()
    server.dispose()
    server.dispose()

class \nodoc\ iso _TestDTLSReadAfterDispose is UnitTest
  """
  Reading from a disposed DTLS session returns `InvalidOperation`.
  """
  fun name(): String => "net/dtls/DTLS.read/after_dispose"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestDTLSSessionPair(h)?
      else
        h.fail("could not establish a DTLS session pair")
        return
      end

    client.dispose()

    h.assert_true(
      client.read() is InvalidOperation,
      "read after dispose should return InvalidOperation")
    h.assert_true(
      client.read(10) is InvalidOperation,
      "read(10) after dispose should return InvalidOperation")

    server.dispose()

class \nodoc\ iso _TestDTLSReceiveAfterDispose is UnitTest
  """
  Receiving on a disposed DTLS session returns `InvalidOperation`.
  """
  fun name(): String => "net/dtls/DTLS.receive/after_dispose"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestDTLSSessionPair(h)?
      else
        h.fail("could not establish a DTLS session pair")
        return
      end

    client.dispose()

    h.assert_true(
      client.receive("hello") is InvalidOperation,
      "receive after dispose should return InvalidOperation")

    server.dispose()

class \nodoc\ iso _TestDTLSContextDisposeTwice is UnitTest
  """
  Disposing a DTLSContext twice does not crash.
  """
  fun name(): String => "net/dtls/DTLSContext.dispose/twice"

  fun apply(h: TestHelper) =>
    let ctx = DTLSContext

    ctx
      .> dispose()
      .dispose()

class \nodoc\ iso _TestDTLSContextClientAfterDispose is UnitTest
  """
  `client` on a disposed DTLSContext raises an error rather than handing a null
  context to `SSL_new`.
  """
  fun name(): String => "net/dtls/DTLSContext.client/after_dispose"

  fun apply(h: TestHelper) =>
    let live: DTLSContext val = recover val DTLSContext end

    try
      live.client()?.dispose()
    else
      h.fail("client() on a live context should not raise")
    end

    let mutable: DTLSContext iso = recover iso DTLSContext end
    mutable.dispose()
    let disposed: DTLSContext val = consume mutable

    try
      disposed.client()?.dispose()
      h.fail("client() on a disposed context should raise")
    end

class \nodoc\ iso _TestDTLSContextServerAfterDispose is UnitTest
  """
  `server` on a disposed DTLSContext raises an error rather than handing a null
  context to `SSL_new`.
  """
  fun name(): String => "net/dtls/DTLSContext.server/after_dispose"

  fun apply(h: TestHelper) =>
    let live: DTLSContext val = recover val DTLSContext end

    try
      live.server()?.dispose()
    else
      h.fail("server() on a live context should not raise")
    end

    let mutable: DTLSContext iso = recover iso DTLSContext end
    mutable.dispose()
    let disposed: DTLSContext val = consume mutable

    try
      disposed.server()?.dispose()
      h.fail("server() on a disposed context should raise")
    end

class \nodoc\ iso _TestDTLSContextSetMinProtoVersionAfterDispose is UnitTest
  """
  `set_min_proto_version` on a disposed DTLSContext raises an error rather than
  passing a null `SSL_CTX*` to `SSL_CTX_ctrl`.
  """
  fun name(): String =>
    "net/dtls/DTLSContext.set_min_proto_version/after_dispose"

  fun apply(h: TestHelper) =>
    let ctx = DTLSContext

    try
      ctx.set_min_proto_version(DTLS1u2Version())?
    else
      h.fail("set_min_proto_version() on a live context should not raise")
    end

    ctx.dispose()

    try
      ctx.set_min_proto_version(DTLS1u2Version())?
      h.fail("set_min_proto_version() on a disposed context should raise")
    end

class \nodoc\ iso _TestDTLSContextSetMaxProtoVersionAfterDispose is UnitTest
  """
  `set_max_proto_version` on a disposed DTLSContext raises an error rather than
  passing a null `SSL_CTX*` to `SSL_CTX_ctrl`.
  """
  fun name(): String =>
    "net/dtls/DTLSContext.set_max_proto_version/after_dispose"

  fun apply(h: TestHelper) =>
    let ctx = DTLSContext

    try
      ctx.set_max_proto_version(DTLS1u2Version())?
    else
      h.fail("set_max_proto_version() on a live context should not raise")
    end

    ctx.dispose()

    try
      ctx.set_max_proto_version(DTLS1u2Version())?
      h.fail("set_max_proto_version() on a disposed context should raise")
    end

class \nodoc\ iso _TestDTLSContextSetAuthorityAfterDispose is UnitTest
  """
  `set_authority` on a disposed DTLSContext raises an error.
  """
  fun name(): String => "net/dtls/DTLSContext.set_authority/after_dispose"

  fun apply(h: TestHelper) =>
    let auth = FileAuth(h.env.root)
    let ctx = DTLSContext

    try
      ctx.set_authority(FilePath(auth, "assets/cert.pem"))?
    else
      h.fail("set_authority() on a live context should not raise")
    end

    ctx.dispose()

    try
      ctx.set_authority(FilePath(auth, "assets/cert.pem"))?
      h.fail("set_authority() on a disposed context should raise")
    end

class \nodoc\ iso _TestDTLSContextSetCertAfterDispose is UnitTest
  """
  `set_cert` on a disposed DTLSContext raises an error.
  """
  fun name(): String => "net/dtls/DTLSContext.set_cert/after_dispose"

  fun apply(h: TestHelper) =>
    let auth = FileAuth(h.env.root)
    let cert = FilePath(auth, "assets/cert.pem")
    let key = FilePath(auth, "assets/key.pem")
    let ctx = DTLSContext

    try
      ctx.set_cert(cert, key)?
    else
      h.fail("set_cert() on a live context should not raise")
    end

    ctx.dispose()

    try
      ctx.set_cert(cert, key)?
      h.fail("set_cert() on a disposed context should raise")
    end

class \nodoc\ iso _TestDTLSContextSetCiphersAfterDispose is UnitTest
  """
  `set_ciphers` on a disposed DTLSContext raises an error.
  """
  fun name(): String => "net/dtls/DTLSContext.set_ciphers/after_dispose"

  fun apply(h: TestHelper) =>
    let ctx = DTLSContext

    try
      ctx.set_ciphers("HIGH")?
    else
      h.fail("set_ciphers(\"HIGH\") on a live context should not raise")
    end

    ctx.dispose()

    try
      ctx.set_ciphers("HIGH")?
      h.fail("set_ciphers() on a disposed context should raise")
    end

class \nodoc\ iso _TestDTLSContextSetVerifyDepthAfterDispose is UnitTest
  """
  `set_verify_depth` on a disposed DTLSContext does nothing.
  """
  fun name(): String => "net/dtls/DTLSContext.set_verify_depth/after_dispose"

  fun apply(h: TestHelper) =>
    let ctx = DTLSContext

    ctx.set_verify_depth(4)

    ctx.dispose()
    ctx.set_verify_depth(4)

    h.assert_false(
      ctx.alpn_set_client_protocols(["h2"]),
      "the context should still be disposed")

class \nodoc\ iso _TestDTLSContextALPNSetResolverAfterDispose is UnitTest
  """
  `alpn_set_resolver` on a disposed DTLSContext returns `false`.
  """
  fun name(): String =>
    "net/dtls/DTLSContext.alpn_set_resolver/after_dispose"

  fun apply(h: TestHelper) =>
    let resolver = ALPNStandardProtocolResolver(["h2"])
    let ctx = DTLSContext

    h.assert_true(
      ctx.alpn_set_resolver(resolver),
      "alpn_set_resolver() on a live context should return true")

    ctx.dispose()

    h.assert_false(
      ctx.alpn_set_resolver(resolver),
      "alpn_set_resolver() on a disposed context should return false")

class \nodoc\ iso _TestDTLSContextALPNSetClientProtocolsAfterDispose
  is UnitTest
  """
  `alpn_set_client_protocols` on a disposed DTLSContext returns `false`.
  """
  fun name(): String =>
    "net/dtls/DTLSContext.alpn_set_client_protocols/after_dispose"

  fun apply(h: TestHelper) =>
    let ctx = DTLSContext

    h.assert_true(
      ctx.alpn_set_client_protocols(["h2"]),
      "alpn_set_client_protocols() on a live context should return true")

    ctx.dispose()

    h.assert_false(
      ctx.alpn_set_client_protocols(["h2"]),
      "alpn_set_client_protocols() on a disposed context should return false")

class \nodoc\ iso _TestDTLSWriteAfterDispose is UnitTest
  """
  `write` on a disposed DTLS session does nothing and does not raise.
  """
  fun name(): String => "net/dtls/DTLS.write/after_dispose"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestDTLSSessionPair(h)?
      else
        h.fail("could not establish a DTLS session pair")
        return
      end

    client.dispose()

    try
      client.write("data")?
    else
      h.fail("write() on a disposed session should not raise an error")
    end

    h.assert_true(
      client.send() is None,
      "write() on a disposed session should not queue anything to send")

    server.dispose()

class \nodoc\ iso _TestDTLSSendAfterDisposeReturnsNone is UnitTest
  """
  `send` on a disposed DTLS session returns `None` instead of reading a freed
  BIO.

  The session has encrypted bytes waiting when it is disposed, so `None` here
  is the disposed check and not an empty BIO.
  """
  fun name(): String => "net/dtls/DTLS.send/after_dispose_returns_none"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestDTLSSessionPair(h)?
      else
        h.fail("could not establish a DTLS session pair")
        return
      end

    try
      client.write("data")?
    else
      h.fail("client could not write application data")
      client.dispose()
      server.dispose()
      return
    end

    h.assert_true(
      client.send() isnt None,
      "a session that has just written should have bytes to send")

    client.dispose()

    h.assert_true(
      client.send() is None,
      "send() on a disposed session should return None")

    server.dispose()

class \nodoc\ iso _TestDTLSReadAfterDisposeWithBufferedFrame is UnitTest
  """
  A session holding decrypted bytes from an incomplete `expect` frame returns
  `InvalidOperation` from `read` once it is disposed, rather than handing
  those bytes back.
  """
  fun name(): String => "net/dtls/DTLS.read/after_dispose_with_buffered_frame"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestDTLSSessionPair(h)?
      else
        h.fail("could not establish a DTLS session pair")
        return
      end

    try
      client.write("ab")?
      _TestDTLSTransfer(client, server)
    else
      h.fail("client could not send application data")
      client.dispose()
      server.dispose()
      return
    end

    h.assert_true(
      server.read(4) is None,
      "two bytes should not satisfy read(4)")

    server.dispose()

    h.assert_true(
      server.read(2) is InvalidOperation,
      "read(2) on a disposed session should return " +
        "InvalidOperation, even with two bytes already buffered")

    client.dispose()

class \nodoc\ iso _TestDTLSReadOnAuthFail is UnitTest
  """
  `read` on a session that failed authentication returns `SSLError` and does
  not lose the authentication failure.
  """
  fun name(): String => "net/dtls/DTLS.read/on_auth_fail"

  fun apply(h: TestHelper) =>
    (let client, let server, let cr, _) =
      try
        let client_ctx =
          _TestDTLSContext(h where authority = true, client_verify = true)?
        let server_ctx = _TestDTLSContext(h where cert = true)?
        _TestDTLSSessionPair.attempt(
          h, client_ctx, server_ctx where hostname = "nomatch.example.com")?
      else
        h.fail("could not create a DTLS session pair")
        return
      end

    h.assert_true(
      cr is SSLAuthFail,
      "the client should report SSLAuthFail after a hostname mismatch")

    h.assert_true(
      client.read() is SSLError,
      "read() on a failed session should return SSLError")

    h.assert_true(
      client.read(10) is SSLError,
      "read(10) on a failed session should return SSLError")

    h.assert_true(
      client.receive("") is SSLAuthFail,
      "receive should still report SSLAuthFail after reads")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestDTLSReceiveOnAuthFail is UnitTest
  """
  `receive` on a session in `SSLAuthFail` does nothing.
  """
  fun name(): String => "net/dtls/DTLS.receive/on_auth_fail"

  fun apply(h: TestHelper) =>
    (let client, let server, let cr, let sr) =
      try
        let client_ctx =
          _TestDTLSContext(h where authority = true, client_verify = true)?
        let server_ctx = _TestDTLSContext(h where cert = true)?
        _TestDTLSSessionPair.attempt(
          h, client_ctx, server_ctx where hostname = "nomatch.example.com")?
      else
        h.fail("could not create a DTLS session pair")
        return
      end

    h.assert_true(
      cr is SSLAuthFail,
      "the client should report SSLAuthFail after a hostname mismatch")
    h.assert_true(
      sr is SSLReady,
      "the server should report SSLReady")

    try
      server.write("should not be received")?
      while true do
        match \exhaustive\ server.send()
        | let d: Array[U8] iso => client.receive(consume d)
        | None => break
        end
      end
    else
      h.fail("server could not produce ciphertext")
      client.dispose()
      server.dispose()
      return
    end

    h.assert_true(
      client.read() is SSLError,
      "data received after auth failure should not be readable")

    h.assert_true(
      client.receive("") is SSLAuthFail,
      "the session should still report SSLAuthFail")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestDTLSSendOnFailedSession is UnitTest
  """
  `send` on a failed session does not crash.

  DTLS does not always queue alert bytes the way TLS does — a hostname
  mismatch is detected after the handshake succeeds at the TLS layer, so
  OpenSSL may have no alert to send. The test verifies that `send` is safe
  to call and returns `None` when there is nothing queued.
  """
  fun name(): String => "net/dtls/DTLS.send/on_failed_session"

  fun apply(h: TestHelper) =>
    (let client, let server, let cr, _) =
      try
        let client_ctx =
          _TestDTLSContext(h where authority = true, client_verify = true)?
        let server_ctx = _TestDTLSContext(h where cert = true)?
        _TestDTLSSessionPair.attempt(
          h, client_ctx, server_ctx where hostname = "nomatch.example.com")?
      else
        h.fail("could not create a DTLS session pair")
        return
      end

    h.assert_true(
      cr is SSLAuthFail,
      "client should report SSLAuthFail")

    // Drain whatever send has — it may be alert bytes or None.
    while true do
      match \exhaustive\ client.send()
      | let _: Array[U8] iso => None
      | None => break
      end
    end

    h.assert_true(
      client.send() is None,
      "send should return None once drained")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestDTLSCloseFromReady is UnitTest
  """
  Calling `close` on a ready session queues a `close_notify` alert in the
  output BIO.
  """
  fun name(): String => "net/dtls/DTLS.close/from_ready"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestDTLSSessionPair(h)?
      else
        h.fail("could not establish a DTLS session pair")
        return
      end

    h.assert_true(
      client.send() is None,
      "nothing queued before close")

    client.close()

    h.assert_true(
      client.send() isnt None,
      "close_notify should be queued in the output BIO")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestDTLSCloseIdempotent is UnitTest
  """
  A second `close` call is a no-op — it produces no additional output.
  """
  fun name(): String => "net/dtls/DTLS.close/idempotent"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestDTLSSessionPair(h)?
      else
        h.fail("could not establish a DTLS session pair")
        return
      end

    client.close()

    while true do
      match \exhaustive\ client.send()
      | let _: Array[U8] iso => None
      | None => break
      end
    end

    client.close()

    h.assert_true(
      client.send() is None,
      "second close should produce no additional output")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestDTLSCloseFromWrongStates is UnitTest
  """
  `close` is a no-op during handshake and after auth failure.

  The SSL equivalent also tests close after SSLError, but DTLS silently drops
  invalid datagrams per RFC 6347 rather than transitioning to SSLError, so
  there is no clean way to reach that state from external input.
  """
  fun name(): String => "net/dtls/DTLS.close/from_wrong_states"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestDTLSSessionPair.fresh(h)?
      else
        h.fail("could not create fresh sessions")
        return
      end

    client.close()

    h.assert_true(
      client.receive("") is SSLAccepted,
      "close during handshake should leave session still handshaking")

    client.dispose()
    server.dispose()

    (let auth_client, let auth_server, let acr, _) =
      try
        _TestDTLSSessionPair.attempt(
          h,
          _TestDTLSContext(h where client_verify = true)?,
          _TestDTLSContext(h where cert = true)?)?
      else
        h.fail("could not create an auth-fail session pair")
        return
      end

    h.assert_true(
      acr is SSLAuthFail,
      "client should report SSLAuthFail")

    auth_client.close()

    h.assert_true(
      auth_client.receive("") is SSLAuthFail,
      "receive should still report SSLAuthFail after close")

    auth_client.dispose()
    auth_server.dispose()

class \nodoc\ iso _TestDTLSCloseAfterDispose is UnitTest
  """
  `close` after `dispose` is a no-op.
  """
  fun name(): String => "net/dtls/DTLS.close/after_dispose"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestDTLSSessionPair(h)?
      else
        h.fail("could not establish a DTLS session pair")
        return
      end

    client.dispose()
    client.close()

    h.assert_true(
      client.send() is None,
      "close after dispose should not queue anything")

    server.dispose()

class \nodoc\ iso _TestDTLSPeerCloseNotifyDetected is UnitTest
  """
  When one session sends `close_notify`, the other detects it via `read` and
  transitions to `SSLClosed`, not `SSLError`.
  """
  fun name(): String => "net/dtls/DTLS.read/peer_close_notify"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestDTLSSessionPair(h)?
      else
        h.fail("could not establish a DTLS session pair")
        return
      end

    client.close()

    _TestDTLSTransfer(client, server)

    match \exhaustive\ server.read()
    | let _: Array[U8] iso =>
      h.fail("server read produced data from a close_notify")
    | None =>
      h.fail("server should report SSLClosed, not None")
    | SSLClosed => None
    | SSLError =>
      h.fail("server should report SSLClosed, not SSLError")
    | InvalidOperation =>
      h.fail("server should report SSLClosed, not InvalidOperation")
    end

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestDTLSWriteBlockedInClosed is UnitTest
  """
  `write` raises an error in `SSLClosed`.
  """
  fun name(): String => "net/dtls/DTLS.write/blocked_in_closed"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestDTLSSessionPair(h)?
      else
        h.fail("could not establish a DTLS session pair")
        return
      end

    client.close()

    let write_succeeded =
      try
        client.write("should fail")?
        true
      else
        false
      end

    h.assert_false(
      write_succeeded,
      "write should raise an error in SSLClosed")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestDTLSBidirectionalCloseRoundtrip is UnitTest
  """
  Full bidirectional shutdown: one side calls `close`, the other detects it
  via `read` and calls `close` in response.
  """
  fun name(): String => "net/dtls/DTLS.close/bidirectional_roundtrip"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestDTLSSessionPair(h)?
      else
        h.fail("could not establish a DTLS session pair")
        return
      end

    client.close()

    _TestDTLSTransfer(client, server)

    let sr = server.read()

    h.assert_true(
      sr is SSLClosed,
      "server should report SSLClosed after receiving client's close_notify")

    server.close()

    _TestDTLSTransfer(server, client)

    let cr = client.read()

    h.assert_true(
      cr is SSLClosed,
      "client should report SSLClosed")

    client.dispose()
    server.dispose()

class \nodoc\ iso _TestDTLSWriteBlockedInClosing is UnitTest
  """
  `write` raises an error in `_DTLSClosing`.
  """
  fun name(): String => "net/dtls/DTLS.write/blocked_in_closing"

  fun apply(h: TestHelper) =>
    (let client, let server) =
      try
        _TestDTLSSessionPair(h)?
      else
        h.fail("could not establish a DTLS session pair")
        return
      end

    client.close()

    _TestDTLSTransfer(client, server)

    let sr = server.read()

    h.assert_true(
      sr is SSLClosed,
      "server should report SSLClosed after receiving peer's close_notify")

    let write_succeeded =
      try
        server.write("should fail")?
        true
      else
        false
      end

    h.assert_false(
      write_succeeded,
      "write should raise an error in _DTLSClosing")

    client.dispose()
    server.dispose()


