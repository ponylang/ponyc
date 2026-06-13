use "files"
use "pony_test"
use "time"

use @pony_os_ip_string[Pointer[U8]](src: Pointer[U8] tag, len: I32)
use @if_indextoname[Pointer[U8]](ifindex: U32, ifname: Pointer[U8] tag)

primitive TimeoutValue
  fun apply(): U64 =>
    ifdef windows then
      // Windows networking is just damn slow at many things
      60_000_000_000
    else
      30_000_000_000
    end

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically
    test(_TestDNSBroadcastIP4)
    test(_TestDNSBroadcastIP6)
    test(_TestOsIpString)
    test(_TestSocketResultDecoder)
    test(_TestTCPConnectionFailed)
    test(_TestTCPExpect)
    test(_TestTCPExpectOverBufferSize)
    test(_TestTCPExpectSetToZero)
    test(_TestTCPMute)
    test(_TestTCPProxy)
    test(_TestTCPUnmute)
    test(_TestTCPWritev)
    test(_TestUnicastIP6Loopback)

    // Tests below run only on linux and are listed alphabetically
    ifdef linux then
      test(_TestBroadcastReceive)
      test(_TestUDPCloseOnSendFailure)
    end

    // Tests below exclude windows and are listed alphabetically
    ifdef not windows then
      test(_TestTCPConnectionToClosedServerFailed)
      test(_TestTCPThrottle)
      test(_TestUDPListenFailure)
    end

    // Tests below run only on linux and freebsd
    ifdef linux or freebsd then
      // macOS, DragonFly, and OpenBSD: the lo0 multicast send fails in
      // the GitHub Actions CI environment (socket closed before
      // delivery); see _TestMulticastIP6's docstring.
      test(_TestMulticastIP6)
    end

    // Tests below exclude osx and are listed alphabetically
    ifdef not osx then
      test(_TestBroadcast)
    end

class \nodoc\ _TestPing is UDPNotify
  let _h: TestHelper
  let _ip: NetAddress

  new create(h: TestHelper, ip: NetAddress) =>
    _h = h

    _ip = try
      let auth = DNSAuth(h.env.root)
      (_, let service) = ip.name()?

      let list = ifdef bsd then
        DNS.ip4(auth, "", service)
      else
        DNS.broadcast_ip4(auth, service)
      end

      let addr = list(0)?

      // Pin the destination actually used for the ping. Without this, a
      // broadcast_ip4 regression (or a quiet retargeting of this test) is
      // masked by the pong socket's INADDR_ANY bind, which completes the
      // test on any datagram reaching the port.
      _h.assert_true(addr.ip4())
      _h.assert_eq[U16](addr.port(), service.u16()?)
      ifdef bsd then
        // The empty-host result is rewritten by the runtime's
        // map_any_to_loopback, deterministically 127.0.0.1.
        _h.assert_eq[U32](addr.ipv4_addr(), 0x7F00_0001)
      else
        _h.assert_eq[U32](addr.ipv4_addr(), U32.max_value())
      end

      addr
    else
      // Coarse on purpose: this else also covers the port-conversion
      // error path above, which is unreachable for the numeric service
      // strings name() produces.
      _h.fail("Couldn't make or verify broadcast address")
      ip
    end

  fun ref not_listening(sock: UDPSocket ref) =>
    _h.fail_action("ping listen")

  fun ref listening(sock: UDPSocket ref) =>
    _h.complete_action("ping listen")

    sock.set_broadcast(true)
    sock.write("ping!", _ip)

    // UDP is best-effort, so a one-shot ping fails the test on any dropped
    // datagram. Retransmit until the test completes: passing still requires
    // a successful send to the broadcast address and a reply. Duplicate
    // replies are harmless - completing an already-completed action is a
    // no-op. The timer is disposed at test teardown; a firing that races
    // teardown either sends one last harmless datagram or writes to the
    // closed socket, which drops it.
    let udp: UDPSocket tag = sock
    let to = _ip
    let timers = Timers
    timers(Timer(
      object iso is TimerNotify
        fun ref apply(timer: Timer, count: U64): Bool =>
          udp.write("ping!", to)
          true
      end,
      250_000_000, 250_000_000))
    _h.dispose_when_done(timers)

  fun ref received(
    sock: UDPSocket ref,
    data: Array[U8] iso,
    from: NetAddress)
  =>
    _h.complete_action("ping receive")

    let s = String .> append(consume data)
    _h.assert_eq[String box](s, "pong!")
    _h.complete(true)

class \nodoc\ _TestPong is UDPNotify
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  fun ref not_listening(sock: UDPSocket ref) =>
    _h.fail_action("pong listen")

  fun ref listening(sock: UDPSocket ref) =>
    _h.complete_action("pong listen")

    sock.set_broadcast(true)
    let ip = sock.local_address()

    let h = _h
    _h.dispose_when_done(
      UDPSocket.ip4(UDPAuth(h.env.root), recover _TestPing(h, ip) end))

  fun ref received(
    sock: UDPSocket ref,
    data: Array[U8] iso,
    from: NetAddress)
  =>
    _h.complete_action("pong receive")

    let s = String .> append(consume data)
    _h.assert_eq[String box](s, "ping!")
    sock.writev(
      recover val [[U8('p'); U8('o'); U8('n'); U8('g'); U8('!')]] end,
      from)

class \nodoc\ iso _TestSocketResultDecoder is UnitTest
  """
  Verify _SocketResultDecoder maps every U8 to the expected union variant.
  The wire values 0/1/2 must match the C-side PONY_SOCKET_OK/RETRY/ERROR
  in `src/libponyrt/lang/socket.h`. Anything else falls through to Error
  so unknown C-side values fail closed.
  """
  fun name(): String => "net/SocketResultDecoder"

  fun ref apply(h: TestHelper) =>
    // Anchor the wire contract: the apply() values must equal the
    // PONY_SOCKET_* constants in socket.h.
    h.assert_eq[U8](_SocketResultOk(), 0)
    h.assert_eq[U8](_SocketResultRetry(), 1)
    h.assert_eq[U8](_SocketResultError(), 2)

    // Sweep every U8: 0 → Ok, 1 → Retry, anything else → Error.
    var v: U8 = 0
    while true do
      let r = _SocketResultDecoder(v)
      match v
      | 0 =>
        h.assert_true(r is _SocketResultOk,
          "decoder(" + v.string() + ") should be Ok")
      | 1 =>
        h.assert_true(r is _SocketResultRetry,
          "decoder(" + v.string() + ") should be Retry")
      else
        h.assert_true(r is _SocketResultError,
          "decoder(" + v.string() + ") should be Error")
      end
      if v == U8.max_value() then break end
      v = v + 1
    end

class \nodoc\ iso _TestBroadcast is UnitTest
  """
  Test broadcasting with UDP. A pong socket listens; a ping socket sends
  "ping!" to the IPv4 broadcast address at the pong socket's port (except on
  BSD, where the destination is a local address instead) and the pong socket
  replies "pong!" to the ping socket's address by ordinary unicast. Passing
  proves a successful send to the broadcast address with SO_BROADCAST set,
  plus the reply; the receiving socket is bound to INADDR_ANY, so reception
  itself is not broadcast-specific (net/BroadcastReceive covers that on
  linux). The resolved destination is asserted in _TestPing, so a regression
  in DNS.broadcast_ip4 or a quiet retargeting of the ping cannot keep this
  test green.

  Both sockets are explicitly IPv4. The default UDPSocket constructor binds
  whichever address family getaddrinfo returns first, and IPv6 has no
  broadcast - that path sent to all-nodes multicast (FF02::1) instead, which
  is unreliable on multi-interface hosts. The ping is retransmitted on a
  timer until the test completes, since UDP delivery is best-effort and a
  single dropped datagram would otherwise fail the test.
  """
  fun name(): String => "net/Broadcast"
  fun label(): String => "unreliable-appveyor-osx"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("pong listen")
    h.expect_action("ping listen")
    h.expect_action("pong receive")
    h.expect_action("ping receive")

    h.dispose_when_done(
      UDPSocket.ip4(UDPAuth(h.env.root), recover _TestPong(h) end))

    h.long_test(TimeoutValue())

  fun ref timed_out(h: TestHelper) =>
    h.log("""
      This test may fail if you have a firewall (such as firewalld) running.
      If it does, try re-running the tests with the firewall de-activated, or
      exclude this test by passing the --exclude="net/Broadcast" option.
    """)

class \nodoc\ _TestBroadcastReceiver is UDPNotify
  let _h: TestHelper
  var _expected: String = ""

  new create(h: TestHelper) =>
    _h = h

  fun ref not_listening(sock: UDPSocket ref) =>
    _h.fail_action("broadcast receive listen")

  fun ref listening(sock: UDPSocket ref) =>
    _h.complete_action("broadcast receive listen")

    let ip = sock.local_address()

    // Bind-address pin: a receiver that silently fell back to INADDR_ANY
    // would still receive the broadcast datagram (net/Broadcast's pong
    // socket proves so), quietly voiding this test's discrimination claim.
    // The wildcard case is observable because the runtime rewrites an
    // any-bound sockname to loopback, so this fails loudly. It pins the
    // bind address only -- 0xFFFFFFFF is a byte-swap palindrome, so it
    // makes no byte-order claim (that pin lives in net/DNSBroadcastIP4).
    _h.assert_true(ip.ip4())
    _h.assert_eq[U32](ip.ipv4_addr(), U32.max_value())

    // The port-embedded payload makes a false match require a foreign
    // sender hitting our ephemeral port with a payload naming that same
    // port -- designed out rather than assumed away.
    _expected = "bcast:" + ip.port().string()

    let h = _h
    _h.dispose_when_done(
      UDPSocket.ip4(UDPAuth(h.env.root),
        recover _TestBroadcastSender(h, ip.port(), _expected) end))

  fun ref received(
    sock: UDPSocket ref,
    data: Array[U8] iso,
    from: NetAddress)
  =>
    let s = String .> append(consume data)
    if s == _expected then
      // Not claimed as from-fidelity coverage; consistency with the other
      // receive handlers in this file.
      _h.assert_true(from.ip4())
      // Completing the last expected action completes the test; the
      // delivery action is what makes completion require delivery.
      _h.complete_action("broadcast receive")
    else
      // A 255.255.255.255-bound receiver sees any broadcast datagram that
      // reaches its port; foreign traffic is environmental, not a
      // regression. Keep waiting -- the retransmit timer guarantees a
      // matching datagram if delivery works.
      _h.log("ignoring unexpected datagram (" + s.size().string() +
        " bytes)")
    end

class \nodoc\ _TestBroadcastSender is UDPNotify
  """
  No closed handler: broadcast send is known-good on the linux legs this
  test runs on, so a send-error close is a regression and surfaces as a
  timeout failure.
  """
  let _h: TestHelper
  let _port: U16
  let _payload: String

  new create(h: TestHelper, port: U16, payload: String) =>
    _h = h
    _port = port
    _payload = payload

  fun ref not_listening(sock: UDPSocket ref) =>
    _h.fail_action("broadcast send listen")

  fun ref listening(sock: UDPSocket ref) =>
    _h.complete_action("broadcast send listen")

    try
      let list: Array[NetAddress] val =
        DNS.broadcast_ip4(DNSAuth(_h.env.root), _port.string())
      let dest = list(0)?

      // Anchor the discrimination claim to this test's own destination.
      _h.assert_eq[U32](dest.ipv4_addr(), U32.max_value())

      sock.set_broadcast(true)
      sock.write(_payload, dest)

      // Retransmit until completion: UDP loss is in-spec (see
      // net/Broadcast).
      let udp: UDPSocket tag = sock
      let payload = _payload
      let timers = Timers
      timers(Timer(
        object iso is TimerNotify
          fun ref apply(timer: Timer, count: U64): Bool =>
            udp.write(payload, dest)
            true
        end,
        250_000_000, 250_000_000))
      _h.dispose_when_done(timers)
    else
      // complete(false) so the failure reports immediately instead of
      // burning the long-test timeout on actions that can never complete.
      _h.fail("couldn't resolve broadcast destination")
      _h.complete(false)
    end

class \nodoc\ iso _TestBroadcastReceive is UnitTest
  """
  Broadcast-discriminating delivery: the receiver is bound to
  255.255.255.255 itself, which on Linux receives only broadcast-addressed
  datagrams -- unicast to the same port is not delivered. (Binding a
  specific unicast interface address receives no broadcast at all, so that
  is not a usable discriminator.) Completion therefore proves the datagram
  that arrived was broadcast-addressed -- the proof net/Broadcast cannot
  give, since its receiver binds INADDR_ANY.

  net/Broadcast already makes set_broadcast load-bearing on these legs via
  its send side; what this test adds is the receive-side discrimination.
  Linux-only: bind-to-broadcast semantics are Linux-verified; BSD differs
  and macOS/Windows are unverified.
  """
  fun name(): String => "net/BroadcastReceive"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("broadcast receive listen")
    h.expect_action("broadcast send listen")
    h.expect_action("broadcast receive")

    h.dispose_when_done(
      UDPSocket.ip4(UDPAuth(h.env.root),
        recover _TestBroadcastReceiver(h) end,
        "255.255.255.255"))

    h.long_test(TimeoutValue())

  fun ref timed_out(h: TestHelper) =>
    h.log("""
      This test may fail if you have a firewall (such as firewalld) running.
      If it does, try re-running the tests with the firewall de-activated,
      or exclude this test by passing the --exclude="net/Broadcast" option.
      That prefix also matching this test is deliberate: both tests depend
      on the same broadcast capability.
    """)

class \nodoc\ iso _TestDNSBroadcastIP4 is UnitTest
  """
  DNS.broadcast_ip4 must resolve to exactly 255.255.255.255 at the
  requested port. Before the destination pin in _TestPing, a regression
  here was masked by net/Broadcast's receive side; this test remains the
  only broadcast_ip4 coverage on macOS (where net/Broadcast doesn't run)
  and on BSD (where net/Broadcast's BSD branch bypasses broadcast_ip4).

  The service sweep 1/12345/65534 (0x0001/0x3039/0xFFFE) is byte-order
  asymmetric so a dropped ntohs in NetAddress.port() fails; 0 and 65535
  are excluded as byte-swap palindromes (0x0000/0xFFFF) that cannot catch
  one. One value suffices to pin the ntohs placement -- the service value
  crosses no branch in net code, so a generated property over services
  would exercise libc's parsing, not this package; the extra values
  document the boundary shape.

  The companion 127.0.0.1 resolve pins NetAddress.ipv4_addr() byte order:
  255.255.255.255 (0xFFFFFFFF) is itself a byte-swap palindrome, so only
  the asymmetric 0x7F000001 catches a dropped ntohl.
  """
  fun name(): String => "net/DNSBroadcastIP4"

  fun ref apply(h: TestHelper) =>
    let auth = DNSAuth(h.env.root)

    for (service, port) in
      [as (String, U16): ("1", 1); ("12345", 12345); ("65534", 65534)]
        .values()
    do
      let broadcast: Array[NetAddress] val = DNS.broadcast_ip4(auth, service)
      h.assert_true(broadcast.size() >= 1,
        "broadcast_ip4(" + service + ") resolved no addresses")
      for addr in broadcast.values() do
        h.assert_true(addr.ip4())
        h.assert_false(addr.ip6())
        h.assert_eq[U32](addr.ipv4_addr(), U32.max_value())
        h.assert_eq[U16](addr.port(), port)
      end

      let loopback: Array[NetAddress] val =
        DNS.ip4(auth, "127.0.0.1", service)
      h.assert_true(loopback.size() >= 1,
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
  DNS.broadcast_ip6 must resolve to exactly FF02::1 (the all-nodes
  multicast address) at the requested port. Same sweep rationale as
  net/DNSBroadcastIP4.

  Gate: ::1 is first resolved with no family pinned (DNS.apply). If no
  IPv6 address comes back on linux, the environment has no usable IPv6
  (the resolver applies AI_ADDRCONFIG; true of the glibc containers CI
  uses) and the test passes vacuously with a log line; elsewhere an
  unresolvable ::1 is itself treated as a failure. The gate deliberately
  avoids DNS.ip6: a family-routing regression in ip6/_resolve would break
  a DNS.ip6 gate too and convert this test into a vacuous pass on the
  linux legs, while with the family-0 gate the mix-up reaches the body and
  fails wherever the gate passed.

  Which legs run the strict body is an environment fact, not a code fact
  (no glibc/musl ifdef exists to pin it): if a CI image loses IPv6, the
  body flips to a vacuous pass there with nothing failing, and the skip
  log is visible only under --verbose. The musl container runs the body
  strictly today but is itself drift-exposed; the durable strict legs are
  macOS (per-PR and nightly Intel) and the BSDs (weekly). The other
  IPv6 tests' gates (net/MulticastIP6, net/UnicastIP6Loopback) share this
  drift risk. scope() is deliberately not asserted -- unzoned resolution
  scope is OS-determined, not part of broadcast_ip6's promise.
  """
  fun name(): String => "net/DNSBroadcastIP6"

  fun ref apply(h: TestHelper) =>
    let auth = DNSAuth(h.env.root)

    // Environment gate: runs once, before the sweep.
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
      h.assert_true(list.size() >= 1,
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

class \nodoc\ _TestMulticastIP6Notify is UDPNotify
  let _h: TestHelper
  let _group: String
  var _expected: String = ""
  var _done: Bool = false

  new create(h: TestHelper, group: String) =>
    _h = h
    _group = group

  fun ref not_listening(sock: UDPSocket ref) =>
    _h.fail_action("multicast listen")

  fun ref listening(sock: UDPSocket ref) =>
    _h.complete_action("multicast listen")

    _h.assert_true(sock.local_address().ip6())

    // Execution-only smoke for set_broadcast's IPv6 arm: it joins FF02::1
    // on an unpinned interface and ignores its argument (issue #5472).
    // Delivery in this test does not depend on this call, and no assertion
    // targets its effect -- kernel auto-membership in the all-nodes group
    // makes the join unobservable from here. The IPv6 arm is NOT
    // behaviorally covered.
    sock.set_broadcast(true)

    // Join on the interface carried by the scoped literal's zone id. The
    // `to` argument is resolved by the runtime and its sin6_scope_id
    // selects the join interface, so it must be a scoped literal -- a bare
    // interface name would fail resolution and silently degrade to
    // interface 0.
    sock.multicast_join(_group, _group)

    try
      let port = sock.local_address().port()
      let list: Array[NetAddress] val =
        DNS.ip6(DNSAuth(_h.env.root), _group, port.string())
      // Hard fail on empty: the group-resolution gate in the test's apply
      // already proved this literal resolves here, so an empty result now
      // is a regression, not environment.
      let dest = list(0)?

      _h.assert_true(dest.ip6())
      // Full-tuple pin: all four words nonzero, pairwise distinct, and
      // byte-order asymmetric, so a dropped ntohl on ANY word of
      // ipv6_addr() fails here -- live even on legs whose send is
      // environmentally absorbed in closed() below.
      (let a1, let a2, let a3, let a4) = dest.ipv6_addr()
      _h.assert_eq[U32](a1, 0xFF12_1122)
      _h.assert_eq[U32](a2, 0x3344_5566)
      _h.assert_eq[U32](a3, 0x7788_99AA)
      _h.assert_eq[U32](a4, 0xBBCC_DDEE)
      // Swap-invariant (!= 0) on purpose: scope()'s byte order is suspect;
      // what matters here is that the zone id survived resolution into the
      // NetAddress at all.
      _h.assert_ne[U32](dest.scope(), 0)

      _expected = "mc6:" + port.string()
      sock.write(_expected, dest)

      // Retransmit until completion: UDP loss is in-spec (see
      // net/Broadcast).
      let udp: UDPSocket tag = sock
      let payload = _expected
      let timers = Timers
      timers(Timer(
        object iso is TimerNotify
          fun ref apply(timer: Timer, count: U64): Bool =>
            udp.write(payload, dest)
            true
        end,
        250_000_000, 250_000_000))
      _h.dispose_when_done(timers)
    else
      // complete(false) so the failure reports immediately instead of
      // burning the long-test timeout on actions that can never complete.
      _h.fail("couldn't resolve " + _group + " after the gate proved it")
      _h.complete(false)
    end

  fun ref received(
    sock: UDPSocket ref,
    data: Array[U8] iso,
    from: NetAddress)
  =>
    let s = String .> append(consume data)
    if s == _expected then
      _h.assert_true(from.ip6())
      _done = true
      // Strict-path completion marker: counterfactual runs key on this
      // line (see the test docstring).
      _h.log("mc6 delivered on " + _group)
      // Completing the last expected action completes the test; the
      // delivery action is what makes completion require delivery.
      _h.complete_action("multicast receive")
    else
      _h.log("ignoring unexpected datagram (" + s.size().string() +
        " bytes)")
    end

  fun ref closed(sock: UDPSocket ref) =>
    if not _done then
      ifdef linux then
        // Environmental absorber: on the musl CI leg (and routeless hosts)
        // the multicast send fails ENETUNREACH and the runtime closes the
        // socket. Vacuous on this leg BY DESIGN; the send path stays
        // strict on FreeBSD (weekly tier-3) and on multicast-capable
        // dev machines. complete(true) finishes
        // immediately, without waiting for the outstanding "multicast
        // receive" action.
        _h.log("socket closed before delivery; treating as environmental" +
          " (no IPv6 multicast route)")
        _h.complete(true)
      else
        _h.fail("socket closed before delivery")
        _h.complete(false)
      end
    end

class \nodoc\ iso _TestMulticastIP6 is UnitTest
  """
  Deterministic IPv6 multicast round trip: a single socket joins a
  transient multicast group on an explicitly chosen interface, sends a
  datagram to the scoped group at its own port, and receives it back via
  the default-on IPV6_MULTICAST_LOOP.

  The group is transient (ff12::/16) by necessity, not whim: every
  interface is implicitly a member of the FF02::1 all-nodes group, so an
  FF02::1 round trip succeeds even with the join deleted -- it cannot
  prove the join works. Delivery of a transient group requires the
  explicit join, which makes multicast_join (and the runtime's
  scope-derived interface selection behind it) load-bearing here. The
  group's four 32-bit words are nonzero, pairwise distinct, and byte-order
  asymmetric on purpose; see the tuple pin in the notify.

  Interface/scope handling is explicit throughout because unscoped
  multicast sends fail (EADDRNOTAVAIL) on hosts without a default
  multicast route -- the nondeterminism that made the old accidental IPv6
  coverage flaky. On Linux the loopback interface has no MULTICAST flag,
  so a real interface is scanned for; on FreeBSD lo0 is multicast-capable
  and used directly (verified by tier-3 CI: the full strict round trip
  delivers there). macOS, DragonFly, and OpenBSD are excluded at
  registration: lo0 advertises MULTICAST on all three, yet in the GitHub
  Actions CI environment the send to the group via lo0 fails identically
  on each (socket closed before delivery; PR #5475's first macOS and
  tier-3 runs).

  Environment gates (log-visible vacuous passes, linux only): no usable
  IPv6 at all (glibc docker CI); no candidate interface; scoped-literal
  resolution failure. After the gates, failures are real failures --
  except a pre-delivery socket close on linux, which is the musl docker
  leg's ENETUNREACH (see the notify's closed()). No per-PR CI leg runs
  the full strict round trip (Linux gates or absorbs as above; Windows,
  macOS, DragonFly, and OpenBSD are excluded), so strict delivery
  enforcement lives on the weekly tier-3 FreeBSD legs and on
  multicast-capable dev machines; which legs run strict is an
  environment fact and can drift with CI images (see the drift note in
  net/DNSBroadcastIP6).

  Counterfactual protocol: confirm the "mc6 delivered on" marker appears
  (--verbose) BEFORE trusting any mutation run -- a mutation "timeout"
  against a vacuously-passing baseline proves nothing.
  """
  fun name(): String => "net/MulticastIP6"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    let auth = DNSAuth(h.env.root)

    // Environment pre-gate: any usable IPv6 at all?
    if not _resolves_ip6(auth, "::1") then
      ifdef linux then
        h.log("no usable IPv6 (::1 unresolvable); skipping")
      else
        h.fail("::1 did not resolve to an IPv6 address")
      end
      return
    end

    match _scoped_group()
    | None =>
      ifdef linux then
        h.log("no candidate multicast interface among if_indextoname " +
          "indices 1..64; skipping")
      else
        h.fail("no candidate multicast interface")
      end
    | let group: String =>
      // Group-resolution gate: the scoped literal must resolve here for
      // the body's resolution to be a hard assertion.
      if not _resolves_ip6(auth, group) then
        ifdef linux then
          h.log("scoped group " + group + " unresolvable; skipping")
        else
          h.fail("scoped group " + group + " did not resolve")
        end
        return
      end

      h.log("using scoped group " + group)
      h.expect_action("multicast listen")
      h.expect_action("multicast receive")
      h.dispose_when_done(
        UDPSocket.ip6(UDPAuth(h.env.root),
          recover _TestMulticastIP6Notify(h, group) end))
      h.long_test(TimeoutValue())
    end

  fun ref timed_out(h: TestHelper) =>
    h.log("""
      This test needs an UP, multicast-capable, IPv6-enabled interface. A
      firewall may also block link-local multicast. You can exclude this
      test by passing the --exclude="net/MulticastIP6" option.
    """)

  fun _resolves_ip6(auth: DNSAuth, host: String): Bool =>
    let list: Array[NetAddress] val = DNS.ip6(auth, host, "0")
    list.size() > 0

  fun _scoped_group(): (String | None) =>
    """
    The transient group pinned to a multicast-capable interface, or None
    when no candidate interface exists.
    """
    let group = "ff12:1122:3344:5566:7788:99aa:bbcc:ddee"
    ifdef freebsd then
      // Loopback multicast delivery is verified working on FreeBSD
      // (tier-3 CI); the other lo0 platforms are excluded at
      // registration.
      group + "%lo0"
    elseif linux then
      match _linux_interface()
      | let name': String => group + "%" + name'
      | None => None
      end
    else
      // Never registered elsewhere (windows, osx, dragonfly, and
      // openbsd are excluded at registration).
      None
    end

  fun _linux_interface(): (String | None) =>
    """
    The first interface from if_indextoname indices 1..64 that isn't "lo"
    (the Linux loopback has no MULTICAST flag). Indices are neither
    contiguous nor bounded by 64, but 1..64 covers realistic hosts; a miss
    is a log-visible vacuous pass upstream, never a failure. No
    IFF_MULTICAST check is made: a multicast-incapable pick funnels into
    the notify's closed() absorber, which is also log-visible.
    """
    var i: U32 = 1
    while i <= 64 do
      let name' = recover val
        // if_indextoname requires a buffer of at least IF_NAMESIZE (16)
        // bytes and returns NULL when the index names no interface.
        let buf = Array[U8] .> undefined(16)
        if @if_indextoname(i, buf.cpointer()).is_null() then
          ""
        else
          var len: USize = 0
          for b in buf.values() do
            if b == 0 then break end
            len = len + 1
          end
          buf.truncate(len)
          String .> append(buf)
        end
      end
      if (name' != "") and (name' != "lo") then
        return name'
      end
      i = i + 1
    end
    None

class \nodoc\ _TestCloseOnSendFailureNotify is UDPNotify
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  fun ref not_listening(sock: UDPSocket ref) =>
    _h.fail_action("send failure listen")

  fun ref listening(sock: UDPSocket ref) =>
    _h.complete_action("send failure listen")

    try
      // The destination port is the socket's own; its value is irrelevant
      // because the datagram never leaves -- the send fails EACCES.
      let list: Array[NetAddress] val =
        DNS.broadcast_ip4(DNSAuth(_h.env.root),
          sock.local_address().port().string())
      let dest = list(0)?

      // Deliberately NO set_broadcast(true): sending to the broadcast
      // address without SO_BROADCAST fails EACCES, and the contract under
      // test is _write's error arm -- a failed send closes the socket and
      // delivers closed().
      sock.write("denied", dest)
    else
      // complete(false) so the failure reports immediately instead of
      // burning the long-test timeout on actions that can never complete.
      _h.fail("couldn't resolve broadcast destination")
      _h.complete(false)
    end

  fun ref closed(sock: UDPSocket ref) =>
    _h.complete_action("send failure close")

class \nodoc\ iso _TestUDPCloseOnSendFailure is UnitTest
  """
  A failed send closes the socket and delivers closed() -- the error arm
  of UDPSocket._write. The deterministic trigger: sending to the broadcast
  address without SO_BROADCAST fails EACCES on Linux; this socket
  deliberately never calls set_broadcast.

  Counterfactual: add set_broadcast(true) and the send succeeds, closed()
  never fires during the test, and the test fails by timeout -- the
  closed() expectation is load-bearing.

  Linux-only: the EACCES-without-SO_BROADCAST behavior is Linux-verified.
  No exclusion_group: the socket binds an ephemeral port and its only
  datagram never leaves the host (the send fails at the socket layer), so
  there is no interference surface.
  """
  fun name(): String => "net/UDPCloseOnSendFailure"

  fun ref apply(h: TestHelper) =>
    h.expect_action("send failure listen")
    h.expect_action("send failure close")

    h.dispose_when_done(
      UDPSocket.ip4(UDPAuth(h.env.root),
        recover _TestCloseOnSendFailureNotify(h) end))

    h.long_test(TimeoutValue())

class \nodoc\ _TestListenFailureNotify is UDPNotify
  let _h: TestHelper
  let _label: String

  new create(h: TestHelper, label': String) =>
    _h = h
    _label = label'

  fun ref not_listening(sock: UDPSocket ref) =>
    _h.complete_action(_label)

  fun ref listening(sock: UDPSocket ref) =>
    // The premise broke: this platform produced a listener from a
    // cross-family literal (e.g. a resolver applying AI_V4MAPPED). Log the
    // bound address for diagnosis, then fail this arm immediately
    // (fail_action completes the action as failed; no timeout burn).
    try
      (let host, let port) = sock.local_address().name()?
      _h.log(_label + ": bound to " + host + ":" + port)
    end
    _h.log(_label + ": unexpectedly listening -- re-evaluate the test " +
      "premise on this platform; see the test docstring")
    sock.dispose()
    _h.fail_action(_label)

class \nodoc\ iso _TestUDPListenFailure is UnitTest
  """
  Asserts that not_listening actually fires. It is the only UDPNotify
  callback with no default body -- every user is forced to implement it --
  yet no other test asserts it as an outcome (everywhere else it is a fail
  path).

  Constructor family x literal family is a 2x2 matrix: the valid cells
  are covered elsewhere in this file (ip4+v4 by net/Broadcast and
  net/BroadcastReceive; ip6+v6 by net/MulticastIP6's and
  net/UnicastIP6Loopback's binds); this test covers both invalid cells
  (ip6 with a v4 literal, ip4 with a v6 literal). The triggers are
  implementation-conditioned, deliberately: the runtime resolves with the
  socket's family and
  AI_ADDRCONFIG and never AI_V4MAPPED, so a cross-family literal cannot
  resolve and the listen fails. If dual-stack/AI_V4MAPPED support is ever
  added intentionally, this test fails loudly (listening fires) and must
  be updated -- that is the pin working as intended, not flake.

  Not on Windows: a failed UDP listen there crashes the process before
  anything could be observed (issue #5474). No exclusion_group: neither
  arm ever binds successfully or emits a datagram, so there is no
  interference surface.
  """
  fun name(): String => "net/UDPListenFailure"

  fun ref apply(h: TestHelper) =>
    h.expect_action("ip6 ctor with v4 literal refuses to listen")
    h.expect_action("ip4 ctor with v6 literal refuses to listen")

    h.dispose_when_done(
      UDPSocket.ip6(UDPAuth(h.env.root),
        recover
          _TestListenFailureNotify(h,
            "ip6 ctor with v4 literal refuses to listen")
        end,
        "127.0.0.1"))
    h.dispose_when_done(
      UDPSocket.ip4(UDPAuth(h.env.root),
        recover
          _TestListenFailureNotify(h,
            "ip4 ctor with v6 literal refuses to listen")
        end,
        "::1"))

    h.long_test(TimeoutValue())

class \nodoc\ _TestUnicastIP6Receiver is UDPNotify
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  fun ref not_listening(sock: UDPSocket ref) =>
    _h.fail_action("unicast receive listen")

  fun ref listening(sock: UDPSocket ref) =>
    _h.complete_action("unicast receive listen")

    let ip = sock.local_address()
    _h.assert_true(ip.ip6())
    // Sockname pin: deterministic for a ::1-bound socket; the last word
    // (0x00000001) is byte-order asymmetric. This is the suite's only
    // value-level pin of pony_os_sockname's IPv6 marshaling. Word-order
    // coverage of ipv6_addr() across all four words is delegated to
    // net/MulticastIP6's group tuple.
    (let a1, let a2, let a3, let a4) = ip.ipv6_addr()
    _h.assert_eq[U32](a1, 0)
    _h.assert_eq[U32](a2, 0)
    _h.assert_eq[U32](a3, 0)
    _h.assert_eq[U32](a4, 1)

    let h = _h
    _h.dispose_when_done(
      UDPSocket.ip6(UDPAuth(h.env.root),
        recover _TestUnicastIP6Sender(h, ip) end,
        "::1"))

  fun ref received(
    sock: UDPSocket ref,
    data: Array[U8] iso,
    from: NetAddress)
  =>
    let s = String .> append(consume data)
    _h.assert_eq[String box](s, "ping6!")
    // The sender is explicitly ::1-bound, so the from address is
    // determined by this test's inputs, not OS source selection.
    _h.assert_true(from.ip6())
    (let a1, let a2, let a3, let a4) = from.ipv6_addr()
    _h.assert_eq[U32](a1, 0)
    _h.assert_eq[U32](a2, 0)
    _h.assert_eq[U32](a3, 0)
    _h.assert_eq[U32](a4, 1)
    sock.write("pong6!", from)

class \nodoc\ _TestUnicastIP6Sender is UDPNotify
  let _h: TestHelper
  let _dest: NetAddress

  new create(h: TestHelper, dest: NetAddress) =>
    _h = h
    _dest = dest

  fun ref not_listening(sock: UDPSocket ref) =>
    _h.fail_action("unicast send listen")

  fun ref listening(sock: UDPSocket ref) =>
    _h.complete_action("unicast send listen")

    sock.write("ping6!", _dest)

    // Retransmit until completion: UDP loss is in-spec (see
    // net/Broadcast).
    let udp: UDPSocket tag = sock
    let dest = _dest
    let timers = Timers
    timers(Timer(
      object iso is TimerNotify
        fun ref apply(timer: Timer, count: U64): Bool =>
          udp.write("ping6!", dest)
          true
      end,
      250_000_000, 250_000_000))
    _h.dispose_when_done(timers)

  fun ref received(
    sock: UDPSocket ref,
    data: Array[U8] iso,
    from: NetAddress)
  =>
    let s = String .> append(consume data)
    _h.assert_eq[String box](s, "pong6!")
    // Completing the last expected action completes the test; the echo
    // action is what makes completion require the round trip.
    _h.complete_action("unicast echo")

class \nodoc\ iso _TestUnicastIP6Loopback is UnitTest
  """
  IPv6 unicast echo over ::1: pins sockaddr_in6 marshaling on both
  runtime paths (pony_os_sockname via the receiver's bound-address tuple;
  pony_os_recvfrom via the from address) and reply-to-from fidelity. Both
  sockets bind ::1 explicitly so every asserted address is determined by
  this test's inputs, not by OS source-address selection. The explicit
  host is executed, not pinned: a host-ignoring wildcard bind produces
  the same sockname tuple (map_any_to_loopback rewrites :: to ::1) and
  still receives the echo, so that mutation is not caught here -- the v4
  analogue IS pinned by net/BroadcastReceive's bind assertion.

  No closed handlers: nothing environmental closes these sockets on any
  leg past the gate, so timeout is the failure signal. The receive
  handlers also assert strictly instead of log-and-ignore (unlike
  net/BroadcastReceive and net/MulticastIP6): only loopback-sourced
  datagrams can reach these ::1-bound ephemeral ports, so a payload
  mismatch is our bug, not environment.

  Gate: on linux, an unresolvable ::1 means no usable IPv6 (glibc docker
  CI) -- a logged vacuous pass; elsewhere it is a failure. Which legs run
  the strict body is an environment fact and can drift with CI images
  (see net/DNSBroadcastIP6).
  """
  fun name(): String => "net/UnicastIP6Loopback"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    let auth = DNSAuth(h.env.root)

    let gate: Array[NetAddress] val = DNS.ip6(auth, "::1", "0")
    if gate.size() == 0 then
      ifdef linux then
        h.log("no usable IPv6 (::1 unresolvable); skipping")
      else
        h.fail("::1 did not resolve to an IPv6 address")
      end
      return
    end

    h.expect_action("unicast receive listen")
    h.expect_action("unicast send listen")
    h.expect_action("unicast echo")

    h.dispose_when_done(
      UDPSocket.ip6(UDPAuth(h.env.root),
        recover _TestUnicastIP6Receiver(h) end,
        "::1"))

    h.long_test(TimeoutValue())

class \nodoc\ _TestTCP is TCPListenNotify
  """
  Run a typical TCP test consisting of a single TCPListener that accepts a
  single TCPConnection as a client, using a dynamic available listen port.
  """
  let _h: TestHelper
  var _client_conn_notify: (TCPConnectionNotify iso | None) = None
  var _server_conn_notify: (TCPConnectionNotify iso | None) = None

  new iso create(h: TestHelper) =>
    _h = h

  fun iso apply(c: TCPConnectionNotify iso, s: TCPConnectionNotify iso) =>
    _client_conn_notify = consume c
    _server_conn_notify = consume s

    let h = _h
    h.expect_action("server create")
    h.expect_action("server listen")
    h.expect_action("client create")
    h.expect_action("server accept")

    h.dispose_when_done(TCPListener(TCPListenAuth(h.env.root), consume this))
    h.complete_action("server create")

    h.long_test(TimeoutValue())

  fun ref not_listening(listen: TCPListener ref) =>
    _h.fail_action("server listen")

  fun ref listening(listen: TCPListener ref) =>
    _h.complete_action("server listen")

    try
      let notify = (_client_conn_notify = None) as TCPConnectionNotify iso^
      (let host, let port) = listen.local_address().name()?
      _h.dispose_when_done(
        TCPConnection(TCPConnectAuth(_h.env.root), consume notify, host, port))
      _h.complete_action("client create")
    else
      _h.fail_action("client create")
    end

  fun ref connected(listen: TCPListener ref): TCPConnectionNotify iso^ ? =>
    try
      let notify = (_server_conn_notify = None) as TCPConnectionNotify iso^
      _h.complete_action("server accept")
      consume notify
    else
      _h.fail_action("server accept")
      error
    end

class \nodoc\ iso _TestTCPExpect is UnitTest
  """
  Test expecting framed data with TCP.
  """
  fun name(): String => "net/TCP.expect"
  fun label(): String => "unreliable-osx"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("client connect")
    h.expect_action("client receive")
    h.expect_action("server receive")
    h.expect_action("expect received")

    _TestTCP(h)(_TestTCPExpectNotify(h, false), _TestTCPExpectNotify(h, true))

class \nodoc\ iso _TestTCPExpectOverBufferSize is UnitTest
  """
  Test expecting framed data with TCP.
  """
  fun name(): String => "net/TCP.expectoverbuffersize"
  fun label(): String => "unreliable-osx"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("client connect")
    h.expect_action("connected")
    h.expect_action("accepted")

    _TestTCP(h)(_TestTCPExpectOverBufferSizeNotify(h),
      _TestTCPExpectOverBufferSizeNotify(h))

class \nodoc\ _TestTCPExpectNotify is TCPConnectionNotify
  let _h: TestHelper
  let _server: Bool
  var _expect: USize = 4
  var _frame: Bool = true

  new iso create(h: TestHelper, server: Bool) =>
    _server = server
    _h = h

  fun ref accepted(conn: TCPConnection ref) =>
    conn.set_nodelay(true)
    try
      conn.expect(_expect)?
      _send(conn, "hi there")
    else
      _h.fail("expect threw an error")
    end

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("client connect failed")

  fun ref connected(conn: TCPConnection ref) =>
    _h.complete_action("client connect")
    conn.set_nodelay(true)
    try
      conn.expect(_expect)?
    else
      _h.fail("expect threw an error")
    end

  fun ref expect(conn: TCPConnection ref, qty: USize): USize =>
    _h.complete_action("expect received")
    qty

  fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] val,
    times: USize)
    : Bool
  =>
    if _frame then
      _frame = false
      _expect = 0

      for i in data.values() do
        _expect = (_expect << 8) + i.usize()
      end
    else
      _h.assert_eq[USize](_expect, data.size())

      if _server then
        _h.complete_action("server receive")
        _h.assert_eq[String](String.from_array(data), "goodbye")
      else
        _h.complete_action("client receive")
        _h.assert_eq[String](String.from_array(data), "hi there")
        _send(conn, "goodbye")
      end

      _frame = true
      _expect = 4
    end

    try
      conn.expect(_expect)?
    else
      _h.fail("expect threw an error")
    end
    true

  fun ref _send(conn: TCPConnection ref, data: String) =>
    let len = data.size()

    var buf = recover Array[U8] end
    buf.push((len >> 24).u8())
    buf.push((len >> 16).u8())
    conn.write(consume buf)

    buf = recover Array[U8] end
    buf.push((len >> 8).u8())
    buf.push((len >> 0).u8())
    buf.append(data)
    conn.write(consume buf)

class \nodoc\ _TestTCPExpectOverBufferSizeNotify is TCPConnectionNotify
  let _h: TestHelper
  let _expect: USize = 6_000_000_000

  new iso create(h: TestHelper) =>
    _h = h

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("client connect failed")

  fun ref accepted(conn: TCPConnection ref) =>
    conn.set_nodelay(true)
    try
      conn.expect(_expect)?
      _h.fail("expect didn't error out")
    else
      _h.complete_action("accepted")
    end

  fun ref connected(conn: TCPConnection ref) =>
    _h.complete_action("client connect")
    conn.set_nodelay(true)
    try
      conn.expect(_expect)?
      _h.fail("expect didn't error out")
    else
      _h.complete_action("connected")
    end

class \nodoc\ iso _TestTCPExpectSetToZero is UnitTest
  """
  Test that after reading with a non-zero expect, setting expect to 0
  results in all remaining data being delivered.
  """
  fun name(): String => "net/TCP.expectsettozero"
  fun label(): String => "unreliable-osx"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("client connect")
    h.expect_action("client receive")

    _TestTCP(h)(
      _TestTCPExpectSetToZeroClientNotify(h),
      _TestTCPExpectSetToZeroServerNotify(h))

class \nodoc\ _TestTCPExpectSetToZeroServerNotify is TCPConnectionNotify
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref accepted(conn: TCPConnection ref) =>
    conn.set_nodelay(true)
    conn.write("hello world")

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("server connect failed")

class \nodoc\ _TestTCPExpectSetToZeroClientNotify is TCPConnectionNotify
  let _h: TestHelper
  var _first: Bool = true
  var _accumulated: Array[U8] iso = recover Array[U8] end

  new iso create(h: TestHelper) =>
    _h = h

  fun ref connected(conn: TCPConnection ref) =>
    _h.complete_action("client connect")
    conn.set_nodelay(true)
    try
      conn.expect(5)?
    else
      _h.fail("expect threw an error")
    end

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("client connect failed")

  fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] val,
    times: USize)
    : Bool
  =>
    if _first then
      _first = false
      _h.assert_eq[String](String.from_array(data), "hello")

      // Switch to expect(0) — this is the scenario under test
      try
        conn.expect(0)?
      else
        _h.fail("expect(0) threw an error")
      end
    else
      // Accumulate data delivered with expect(0)
      _accumulated.append(data)
      if _accumulated.size() >= 6 then
        let s = String.from_array(
          _accumulated = recover Array[U8] end)
        _h.assert_eq[String](s, " world")
        _h.complete_action("client receive")
      end
    end
    true

class \nodoc\ iso _TestTCPWritev is UnitTest
  """
  Test writev (and sent/sentv notification).
  """
  fun name(): String => "net/TCP.writev"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("client connect")
    h.expect_action("server receive")

    _TestTCP(h)(_TestTCPWritevNotifyClient(h), _TestTCPWritevNotifyServer(h))

class \nodoc\ _TestTCPWritevNotifyClient is TCPConnectionNotify
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref sentv(conn: TCPConnection ref, data: ByteSeqIter): ByteSeqIter =>
    recover
      Array[ByteSeq] .> concat(data.values()) .> push(" (from client)")
    end

  fun ref connected(conn: TCPConnection ref) =>
    _h.complete_action("client connect")
    conn.writev(recover ["hello"; ", hello"] end)

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("client connect failed")

class \nodoc\ _TestTCPWritevNotifyServer is TCPConnectionNotify
  let _h: TestHelper
  var _buffer: String iso = recover iso String end

  new iso create(h: TestHelper) =>
    _h = h

  fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] iso,
    times: USize)
    : Bool
  =>
    _buffer.append(consume data)

    let expected = "hello, hello (from client)"

    if _buffer.size() >= expected.size() then
      let buffer: String = _buffer = recover iso String end
      _h.assert_eq[String](expected, consume buffer)
      _h.complete_action("server receive")
    end
    true

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("sender connect failed")

class \nodoc\ iso _TestTCPMute is UnitTest
  """
  Test that the `mute` behavior stops us from reading incoming data. The
  test assumes that send/recv works correctly and that the absence of
  data received is because we muted the connection.

  Test works as follows:

  Once an incoming connection is established, we set mute on it and then
  verify that within a 2 second long test that received is not called on
  our notifier. A timeout is considering passing and received being called
  is grounds for a failure.
  """
  fun name(): String => "net/TCPMute"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("receiver accepted")
    h.expect_action("sender connected")
    h.expect_action("receiver muted")
    h.expect_action("receiver asks for data")
    h.expect_action("sender sent data")

    _TestTCP(h)(_TestTCPMuteSendNotify(h),
      _TestTCPMuteReceiveNotify(h))

  fun timed_out(h: TestHelper) =>
    h.complete(true)

class \nodoc\ _TestTCPMuteReceiveNotify is TCPConnectionNotify
  """
  Notifier to fail a test if we receive data after muting the connection.
  """
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref accepted(conn: TCPConnection ref) =>
    _h.complete_action("receiver accepted")
    conn.mute()
    _h.complete_action("receiver muted")
    conn.write("send me some data that i won't ever read")
    _h.complete_action("receiver asks for data")
    _h.dispose_when_done(conn)

  fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] val,
    times: USize)
    : Bool
  =>
    _h.complete(false)
    true

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("receiver connect failed")

class \nodoc\ _TestTCPMuteSendNotify is TCPConnectionNotify
  """
  Notifier that sends data back when it receives any. Used in conjunction with
  the mute receiver to verify that after muting, we don't get any data on
  to the `received` notifier on the muted connection. We only send in response
  to data from the receiver to make sure we don't end up failing due to race
  condition where the senders sends data on connect before the receiver has
  executed its mute statement.
  """
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref connected(conn: TCPConnection ref) =>
    _h.complete_action("sender connected")

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("sender connect failed")

   fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] val,
    times: USize)
    : Bool
   =>
     conn.write("it's sad that you won't ever read this")
     _h.complete_action("sender sent data")
     true

class \nodoc\ iso _TestTCPUnmute is UnitTest
  """
  Test that the `unmute` behavior will allow a connection to start reading
  incoming data again. The test assumes that `mute` works correctly and that
  after muting, `unmute` successfully reset the mute state rather than `mute`
  being broken and never actually muting the connection.

  Test works as follows:

  Once an incoming connection is established, we set mute on it, request
  that data be sent to us and then unmute the connection such that we should
  receive the return data.
  """
  fun name(): String => "net/TCPUnmute"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("receiver accepted")
    h.expect_action("sender connected")
    h.expect_action("receiver muted")
    h.expect_action("receiver asks for data")
    h.expect_action("receiver unmuted")
    h.expect_action("sender sent data")

    _TestTCP(h)(_TestTCPMuteSendNotify(h),
      _TestTCPUnmuteReceiveNotify(h))

class \nodoc\ _TestTCPUnmuteReceiveNotify is TCPConnectionNotify
  """
  Notifier to test that after muting and unmuting a connection, we get data
  """
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref accepted(conn: TCPConnection ref) =>
    _h.complete_action("receiver accepted")
    conn.mute()
    _h.complete_action("receiver muted")
    conn.write("send me some data that i won't ever read")
    _h.complete_action("receiver asks for data")
    conn.unmute()
    _h.complete_action("receiver unmuted")

  fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] val,
    times: USize)
    : Bool
  =>
    _h.complete(true)
    true

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("receiver connect failed")

class \nodoc\ iso _TestTCPThrottle is UnitTest
  """
  Test that when we experience backpressure when sending that the `throttled`
  method is called on our `TCPConnectionNotify` instance.

  We do this by starting up a server connection, muting it immediately and then
  sending data to it which should trigger a throttling to happen. We don't
  start sending data til after the receiver has muted itself and sent the
  sender data. This verifies that muting has been completed before any data is
  sent as part of testing throttling.

  This test assumes that muting functionality is working correctly.
  """
  fun name(): String => "net/TCPThrottle"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("receiver accepted")
    h.expect_action("sender connected")
    h.expect_action("receiver muted")
    h.expect_action("receiver asks for data")
    h.expect_action("sender sent data")
    h.expect_action("sender throttled")

    _TestTCP(h)(_TestTCPThrottleSendNotify(h),
      _TestTCPThrottleReceiveNotify(h))

class \nodoc\ _TestTCPThrottleReceiveNotify is TCPConnectionNotify
  """
  Notifier to that mutes itself on startup. We then send data to it in order
  to trigger backpressure on the sender.
  """
  let _h: TestHelper

  new iso create(h: TestHelper) =>
    _h = h

  fun ref accepted(conn: TCPConnection ref) =>
    _h.complete_action("receiver accepted")
    conn.mute()
    _h.complete_action("receiver muted")
    conn.write("send me some data that i won't ever read")
    _h.complete_action("receiver asks for data")
    _h.dispose_when_done(conn)

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("receiver connect failed")

class \nodoc\ _TestTCPThrottleSendNotify is TCPConnectionNotify
  """
  Notifier that sends data back when it receives any. Used in conjunction with
  the mute receiver to verify that after muting, we don't get any data on
  to the `received` notifier on the muted connection. We only send in response
  to data from the receiver to make sure we don't end up failing due to race
  condition where the senders sends data on connect before the receiver has
  executed its mute statement.
  """
  let _h: TestHelper
  var _throttled_yet: Bool = false

  new iso create(h: TestHelper) =>
    _h = h

  fun ref connected(conn: TCPConnection ref) =>
    _h.complete_action("sender connected")

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("sender connect failed")

  fun ref received(
    conn: TCPConnection ref,
    data: Array[U8] val,
    times: USize)
    : Bool
  =>
    conn.write("it's sad that you won't ever read this")
    _h.complete_action("sender sent data")
    true

  fun ref throttled(conn: TCPConnection ref) =>
    _throttled_yet = true
    _h.complete_action("sender throttled")
    _h.complete(true)

  fun ref sent(conn: TCPConnection ref, data: ByteSeq): ByteSeq =>
    if not _throttled_yet then
      conn.write("this is more data that you won't ever read" * 10000)
    end
    data

class \nodoc\ _TestTCPProxy is UnitTest
  """
  Check that the proxy callback is called on creation of a TCPConnection.
  """
  fun name(): String => "net/TCPProxy"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("sender connected")
    h.expect_action("sender proxy request")

    _TestTCP(h)(_TestTCPProxyNotify(h),
      _TestTCPProxyNotify(h))

class \nodoc\ _TestTCPProxyNotify is TCPConnectionNotify
  let _h: TestHelper
  new iso create(h: TestHelper) =>
    _h = h

  fun ref proxy_via(host: String, service: String): (String, String) =>
    _h.complete_action("sender proxy request")
    (host, service)

  fun ref connected(conn: TCPConnection ref) =>
    _h.complete_action("sender connected")

  fun ref connect_failed(conn: TCPConnection ref) =>
    _h.fail_action("sender connect failed")

class \nodoc\ _TestTCPConnectionFailed is UnitTest
  fun name(): String => "net/TCPConnectionFailed"

  fun ref apply(h: TestHelper) =>
    h.expect_action("connection failed")

    let host = ifdef linux then "127.0.0.2" else "127.0.0.1" end
    let port = "7669"

    let connection = TCPConnection(
      TCPConnectAuth(h.env.root),
      object iso is TCPConnectionNotify
        let _h: TestHelper = h

        fun ref connected(conn: TCPConnection ref) =>
          _h.fail_action("connection failed")

        fun ref connect_failed(conn: TCPConnection ref) =>
          _h.complete_action("connection failed")
      end,
      host,
      port)
    h.long_test(TimeoutValue())
    h.dispose_when_done(connection)

class \nodoc\ _TestTCPConnectionToClosedServerFailed is UnitTest
  """
  Check that you can't connect to a closed listener.
  """
  fun name(): String => "net/TCPConnectionToClosedServerFailed"
  fun exclusion_group(): String => "network"

  fun ref apply(h: TestHelper) =>
    h.expect_action("server listening")
    h.expect_action("client connection failed")

    let listener = TCPListener(
      TCPListenAuth(h.env.root),
      object iso is TCPListenNotify
        let _h: TestHelper = h
        var host: String = "?"
        var port: String = "?"

        fun ref listening(listener: TCPListener ref) =>
          _h.complete_action("server listening")
          listener.close()

        fun ref not_listening(listener: TCPListener ref) =>
          _h.fail_action("server listening")

        fun ref closed(listener: TCPListener ref) =>
          _TCPConnectionToClosedServerFailedConnector.connect(_h, host, port)

        fun ref connected(listener: TCPListener ref)
          : TCPConnectionNotify iso^
        =>
          object iso is TCPConnectionNotify
            fun ref received(conn: TCPConnection ref, data: Array[U8] iso,
              times: USize): Bool => true
            fun ref accepted(conn: TCPConnection ref) => None
            fun ref connect_failed(conn: TCPConnection ref) => None
            fun ref closed(conn: TCPConnection ref) => None
          end
      end,
      "127.0.0.1"
    )

    h.dispose_when_done(listener)
    h.long_test(TimeoutValue())

actor \nodoc\ _TCPConnectionToClosedServerFailedConnector
  be connect(h: TestHelper, host: String, port: String) =>
    let connection = TCPConnection(
      TCPConnectAuth(h.env.root),
      object iso is TCPConnectionNotify
        let _h: TestHelper = h

        fun ref connected(conn: TCPConnection ref) =>
          _h.fail_action("client connection failed")

        fun ref connect_failed(conn: TCPConnection ref) =>
          _h.complete_action("client connection failed")
      end,
      host,
      port)
    h.dispose_when_done(connection)

class \nodoc\ _TestOsIpString is UnitTest
  """
  Regression test for https://github.com/ponylang/ponyc/issues/5048.

  pony_os_ip_string had an inverted inet_ntop check that returned NULL
  for valid IP addresses.
  """
  fun name(): String => "net/pony_os_ip_string"

  fun apply(h: TestHelper) =>
    // IPv4: 127.0.0.1
    let ipv4 = [as U8: 0x7F; 0x00; 0x00; 0x01]
    let ipv4_str: String val = recover
      String.from_cstring(@pony_os_ip_string(ipv4.cpointer(), I32(4)))
    end
    h.assert_eq[String](ipv4_str, "127.0.0.1")

    // IPv6: ::1
    let ipv6 = [as U8: 0; 0; 0; 0; 0; 0; 0; 0; 0; 0; 0; 0; 0; 0; 0; 1]
    let ipv6_str: String val = recover
      String.from_cstring(@pony_os_ip_string(ipv6.cpointer(), I32(16)))
    end
    h.assert_eq[String](ipv6_str, "::1")
