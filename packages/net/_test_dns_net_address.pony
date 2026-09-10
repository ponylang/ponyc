use "pony_test"

class \nodoc\ iso _TestDNSBroadcastIP4 is UnitTest
  """
  `DNS.broadcast_ip4` resolves to exactly `255.255.255.255` at the requested
  port. The service sweep `1/12345/65534` is byte-order asymmetric so a
  dropped `ntohs` in `NetAddress.port()` fails; `0` and `65535` are excluded
  as byte-swap palindromes. The companion `127.0.0.1` resolve pins
  `NetAddress.ipv4_addr()` byte order: `0xFFFFFFFF` is itself a palindrome,
  so only the asymmetric `0x7F000001` catches a dropped `ntohl`.
  """
  fun name(): String => "net/DNSBroadcastIP4"

  fun ref apply(h: TestHelper) =>
    let auth = DNSAuth(h.env.root)

    for (service, port) in
      [as (String, U16): ("1", 1); ("12345", 12345); ("65534", 65534)]
        .values()
    do
      let broadcast: Array[NetAddress] val = DNS.broadcast_ip4(auth, service)
      h.assert_true(
        broadcast.size() >= 1,
        "broadcast_ip4(" + service + ") resolved no addresses")
      for addr in broadcast.values() do
        h.assert_true(addr.ip4())
        h.assert_false(addr.ip6())
        h.assert_eq[U32](addr.ipv4_addr(), U32.max_value())
        h.assert_eq[U16](addr.port(), port)
      end

      let loopback: Array[NetAddress] val =
        DNS.ip4(auth, "127.0.0.1", service)
      h.assert_true(
        loopback.size() >= 1,
        "ip4(127.0.0.1, " + service + ") resolved no addresses")
      for addr in loopback.values() do
        h.assert_true(addr.ip4())
        h.assert_false(addr.ip6())
        h.assert_eq[U32](addr.ipv4_addr(), 0x7F00_0001)
        h.assert_eq[U16](addr.port(), port)
      end
    end

class \nodoc\ iso _TestDNSBroadcastIP6 is UnitTest
  """
  `DNS.broadcast_ip6` resolves to exactly `FF02::1` (the all-nodes multicast
  address) at the requested port. Same sweep rationale as `DNSBroadcastIP4`.

  Gate: `::1` is first resolved with no family pinned (`DNS.apply`). If no
  IPv6 address comes back on linux, the environment has no usable IPv6 and
  the test passes vacuously with a log line; elsewhere an unresolvable `::1`
  is a failure. The gate avoids `DNS.ip6`: a family-routing regression in
  `ip6/_resolve` would break a `DNS.ip6` gate too and convert this test into
  a vacuous pass.
  """
  fun name(): String => "net/DNSBroadcastIP6"

  fun ref apply(h: TestHelper) =>
    let auth = DNSAuth(h.env.root)

    var gate_ip6 = false
    let gate: Array[NetAddress] val = DNS(auth, "::1", "1")
    for addr in gate.values() do
      if addr.ip6() then gate_ip6 = true end
    end
    if not gate_ip6 then
      ifdef linux then
        h.log("no usable IPv6 (::1 unresolvable); skipping assertions")
      else
        h.fail("::1 did not resolve to an IPv6 address")
      end
      return
    end

    for (service, port) in
      [as (String, U16): ("1", 1); ("12345", 12345); ("65534", 65534)]
        .values()
    do
      let list: Array[NetAddress] val = DNS.broadcast_ip6(auth, service)
      h.assert_true(
        list.size() >= 1,
        "broadcast_ip6(" + service + ") resolved no addresses")
      for addr in list.values() do
        h.assert_true(addr.ip6())
        h.assert_false(addr.ip4())
        (let a1, let a2, let a3, let a4) = addr.ipv6_addr()
        h.assert_eq[U32](a1, 0xFF02_0000)
        h.assert_eq[U32](a2, 0)
        h.assert_eq[U32](a3, 0)
        h.assert_eq[U32](a4, 1)
        h.assert_eq[U16](addr.port(), port)
      end
    end

class \nodoc\ iso _TestNetAddressNameRoundTripIP4 is UnitTest
  """
  `NetAddress.name()` with numeric host returns the host and service strings,
  and re-resolving them yields the same address. Pins the FFI wiring of
  `pony_os_nameinfo` (the two `iso` out-pointers and the numeric-host flag)
  and the self-consistency of the resolve-name-re-resolve pipeline.

  The exact string pins (`"127.0.0.1"`, `"12345"`) catch a swapped
  host/service out-pointer. The absolute pin (`ipv4_addr() == 0x7F000001`)
  catches byte mangling that `host_eq` -- comparing two identically marshaled
  operands -- cannot see. The negative control (`127.0.0.2`) forces the v4
  arm of `host_eq` to discriminate.
  """
  fun name(): String => "net/NetAddressNameRoundTripIP4"

  fun ref apply(h: TestHelper) =>
    let auth = DNSAuth(h.env.root)
    try
      let resolved: Array[NetAddress] val =
        DNS.ip4(auth, "127.0.0.1", "12345")
      let orig = resolved(0)?
      (let host, let serv) = orig.name()?
      h.assert_eq[String](host, "127.0.0.1")
      h.assert_eq[String](serv, "12345")

      let reresolved: Array[NetAddress] val = DNS.ip4(auth, host, serv)
      let again = reresolved(0)?
      h.assert_true(orig.host_eq(again))
      h.assert_eq[U32](again.ipv4_addr(), 0x7F00_0001)
      h.assert_eq[U16](again.port(), 12345)
      h.assert_true(again.ip4())
      h.assert_false(again.ip6())

      let different: Array[NetAddress] val =
        DNS.ip4(auth, "127.0.0.2", "0")
      h.assert_false(orig.host_eq(different(0)?))
    else
      h.fail("IPv4 name() round trip errored")
    end

class \nodoc\ iso _TestNetAddressNameRoundTripIP6 is UnitTest
  """
  IPv6 analogue of `NetAddressNameRoundTripIP4`: resolve `::1`, round-trip
  through `name()` and re-resolution, assert the same address comes back.

  Gate: `DNS.apply` (family 0), not `DNS.ip6`, so a family-routing
  regression does not silently convert this test to a vacuous pass. On linux
  an unresolvable `::1` means no usable IPv6 -- a logged skip; elsewhere it
  is a failure.
  """
  fun name(): String => "net/NetAddressNameRoundTripIP6"

  fun ref apply(h: TestHelper) =>
    let auth = DNSAuth(h.env.root)

    let gate: Array[NetAddress] val = DNS(auth, "::1", "0")
    if gate.size() == 0 then
      ifdef linux then
        h.log("no usable IPv6 (::1 unresolvable); skipping")
      else
        h.fail("::1 did not resolve to an IPv6 address")
      end
      return
    end

    try
      let resolved: Array[NetAddress] val =
        DNS.ip6(auth, "::1", "12345")
      let orig = resolved(0)?
      (let host, let serv) = orig.name()?
      h.assert_eq[String](host, "::1")
      h.assert_eq[String](serv, "12345")

      let reresolved: Array[NetAddress] val = DNS.ip6(auth, host, serv)
      let again = reresolved(0)?
      h.assert_true(orig.host_eq(again))
      (let w1, let w2, let w3, let w4) = again.ipv6_addr()
      h.assert_eq[U32](w1, 0)
      h.assert_eq[U32](w2, 0)
      h.assert_eq[U32](w3, 0)
      h.assert_eq[U32](w4, 1)
      h.assert_eq[U16](again.port(), 12345)
      h.assert_true(again.ip6())
      h.assert_false(again.ip4())

      let different: Array[NetAddress] val = DNS.ip6(auth, "::2", "0")
      h.assert_false(orig.host_eq(different(0)?))
    else
      h.fail("IPv6 name() round trip errored after a successful gate")
    end

class \nodoc\ iso _TestNetAddressIP6Scope is UnitTest
  """
  `NetAddress.scope()` returns the IPv6 scope zone id in host byte order,
  not byte-swapped. The numeric zone literal `%7` pins an exact value
  independent of which interfaces exist: `getaddrinfo` parses it straight
  into `sin6_scope_id`. 7 is byte-order asymmetric, so a reintroduced
  `ntohl` on `scope()` would fail. An unscoped `::1` pins `scope() == 0`.

  Gate: an unresolvable literal is a logged vacuous pass, not a failure.
  """
  fun name(): String => "net/NetAddressIP6Scope"

  fun ref apply(h: TestHelper) =>
    let auth = DNSAuth(h.env.root)

    let scoped: Array[NetAddress] val = DNS.ip6(auth, "ff12::1%7", "0")
    try
      let addr = scoped(0)?
      h.assert_true(addr.ip6())
      h.assert_eq[U32](addr.scope(), 7)
    else
      h.log("ff12::1%7 unresolvable; skipping")
    end

    let unscoped: Array[NetAddress] val = DNS.ip6(auth, "::1", "0")
    try
      let addr = unscoped(0)?
      h.assert_true(addr.ip6())
      h.assert_eq[U32](addr.scope(), 0)
    else
      h.log("::1 unresolvable; skipping")
    end

class \nodoc\ iso _TestDNSUnresolvableEmpty is UnitTest
  """
  An unresolvable host name resolves to an empty array, not an error. The
  `.invalid` TLD (RFC 6761) never resolves; `getaddrinfo` fails it locally
  with no network query.

  The positive control (`127.0.0.1` resolves) prevents a broken resolver
  from passing the size-0 assertions vacuously.
  """
  fun name(): String => "net/DNSUnresolvableEmpty"

  fun ref apply(h: TestHelper) =>
    let auth = DNSAuth(h.env.root)

    let control: Array[NetAddress] val = DNS.ip4(auth, "127.0.0.1", "0")
    h.assert_true(control.size() > 0, "127.0.0.1 control did not resolve")

    let unresolvable = "nonexistent.invalid"
    let any': Array[NetAddress] val = DNS(auth, unresolvable, "0")
    h.assert_eq[USize](any'.size(), 0, "DNS.apply resolved an .invalid name")
    let v4: Array[NetAddress] val = DNS.ip4(auth, unresolvable, "0")
    h.assert_eq[USize](v4.size(), 0, "DNS.ip4 resolved an .invalid name")
    let v6: Array[NetAddress] val = DNS.ip6(auth, unresolvable, "0")
    h.assert_eq[USize](v6.size(), 0, "DNS.ip6 resolved an .invalid name")
