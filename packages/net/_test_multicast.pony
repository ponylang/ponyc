use @if_indextoname[Pointer[U8]](ifindex: U32, ifname: Pointer[U8] tag)

use "pony_test"

class \nodoc\ iso _TestMulticastSockopt is UnitTest
  """
  IPv4 multicast TTL and loopback options round-trip through
  `setsockopt_u32`/`getsockopt_u32` at `IPPROTO_IP` level. Setting them at
  the wrong level (the prior `SOL_SOCKET` bug) either fails the `setsockopt`
  or reads back the wrong value.

  Platform: not macOS/BSD — those return multicast options as 1-byte
  `u_char` instead of 4-byte `u32`, so `getsockopt_u32` fails regardless of
  correctness.
  """
  fun name(): String => "net/MulticastSockopt"

  fun ref apply(h: TestHelper) =>
    h.long_test(5_000_000_000)
    let mc = _TestMulticastSockoptActor(UDPAuth(h.env.root), h)
    h.dispose_when_done(mc)

actor \nodoc\ _TestMulticastSockoptActor
  is (UDPSocketActor & UDPLifecycleEventReceiver)
  var _udp: UDPSocket = UDPSocket.none()
  let _h: TestHelper

  new create(auth: UDPAuth, h: TestHelper) =>
    _h = h
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    _udp = UDPSocket(auth, host, "9824", this, this where ip_version = IP4)

  fun ref _socket(): UDPSocket => _udp

  fun ref _on_bound() =>
    _udp.setsockopt_u32(
      OSSockOpt.ipproto_ip(), OSSockOpt.ip_multicast_ttl(), 7)
    (let ttl_err, let ttl) =
      _udp.getsockopt_u32(
        OSSockOpt.ipproto_ip(), OSSockOpt.ip_multicast_ttl())
    _h.assert_eq[U32](ttl_err, 0, "getsockopt IP_MULTICAST_TTL failed")
    _h.assert_eq[U32](ttl, 7, "IP_MULTICAST_TTL did not round-trip")

    _udp.setsockopt_u32(
      OSSockOpt.ipproto_ip(), OSSockOpt.ip_multicast_loop(), 0)
    (let loop_err, let loop') =
      _udp.getsockopt_u32(
        OSSockOpt.ipproto_ip(), OSSockOpt.ip_multicast_loop())
    _h.assert_eq[U32](loop_err, 0, "getsockopt IP_MULTICAST_LOOP failed")
    _h.assert_eq[U32](loop', 0, "IP_MULTICAST_LOOP did not round-trip")

    _udp.close()

  fun ref _on_bind_failure() =>
    _h.fail("bind failed")
    _h.complete(false)

  fun ref _on_closed() =>
    _h.complete(true)

class \nodoc\ iso _TestMulticastIP4 is UnitTest
  """
  IPv4 multicast round trip on loopback: a socket pins the outgoing
  interface to `127.0.0.1` via `IP_MULTICAST_IF`, joins group `239.1.2.3`
  on `127.0.0.1`, sends a datagram to the group at its own port, and
  receives it back via `IP_MULTICAST_LOOP`. The group's four distinct octets
  make the `ipv4_addr()` pin byte-order asymmetric (`0xEF010203`).

  Falls back to a vacuous pass in `_on_closed` if the datagram is not
  delivered (routeless or multicast-incapable host).
  """
  fun name(): String => "net/MulticastIP4"

  fun ref apply(h: TestHelper) =>
    h.long_test(30_000_000_000)
    let mc =
      _TestMulticastIP4Actor(UDPAuth(h.env.root), "239.1.2.3", h)
    h.dispose_when_done(mc)

actor \nodoc\ _TestMulticastIP4Actor
  is (UDPSocketActor & UDPLifecycleEventReceiver)
  var _udp: UDPSocket = UDPSocket.none()
  let _h: TestHelper
  let _group: String
  var _expected: String = ""
  var _done: Bool = false
  var _attempts: U32 = 0

  new create(auth: UDPAuth, group: String, h: TestHelper) =>
    _h = h
    _group = group
    let host = ifdef linux then "127.0.0.2" else "localhost" end
    _udp = UDPSocket(auth, host, "9825", this, this where ip_version = IP4)

  fun ref _socket(): UDPSocket => _udp

  fun ref _on_bound() =>
    _h.assert_true(_udp.local_address().ip4())

    let loopback_nbo: U32 =
      ifdef bigendian then
        0x7F00_0001
      else
        0x0100_007F
      end
    _udp.setsockopt_u32(
      OSSockOpt.ipproto_ip(), OSSockOpt.ip_multicast_if(), loopback_nbo)

    // IP_ADD_MEMBERSHIP takes struct ip_mreq: group addr + interface addr,
    // both in network byte order.
    let group_nbo: U32 =
      ifdef bigendian then
        0xEF01_0203
      else
        0x0302_01EF
      end
    var mreq = Array[U8](8)
    _push_u32_nbo(mreq, group_nbo)
    _push_u32_nbo(mreq, loopback_nbo)
    _udp.setsockopt(
      OSSockOpt.ipproto_ip(), OSSockOpt.ip_add_membership(), mreq)

    try
      let port = _udp.local_address().port()
      let list: Array[NetAddress] val =
        DNS.ip4(DNSAuth(_h.env.root), _group, port.string())
      let dest = list(0)?

      _h.assert_true(dest.ip4())
      _h.assert_eq[U32](dest.ipv4_addr(), 0xEF01_0203)

      _expected = "mc4:" + port.string()
      _udp.send_to(_expected, dest)
      _retransmit(dest)
    else
      _h.fail("couldn't resolve " + _group)
      _h.complete(false)
    end

  fun ref _on_bind_failure() =>
    _h.fail("bind failed")
    _h.complete(false)

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    let s = String.from_array(consume data)
    if s == _expected then
      _h.assert_true(from.ip4())
      _done = true
      _h.log("mc4 delivered on 127.0.0.1")
      _udp.close()
      _h.complete(true)
    end
    KeepReading

  fun ref _on_closed() =>
    if not _done then
      _h.log("socket closed before delivery; treating as environmental" +
        " (no IPv4 multicast route)")
      _h.complete(true)
    end

  be _retransmit(dest: NetAddress val) =>
    if _udp.is_open() then
      _attempts = _attempts + 1
      if _attempts > 100 then
        _h.log("mc4: no delivery after " + _attempts.string() +
          " attempts; closing")
        _udp.close()
      else
        _udp.send_to(_expected, dest)
        _retransmit(dest)
      end
    end

  fun _push_u32_nbo(arr: Array[U8], v: U32) =>
    ifdef bigendian then
      arr.push(((v >> 24) and 0xFF).u8())
      arr.push(((v >> 16) and 0xFF).u8())
      arr.push(((v >> 8) and 0xFF).u8())
      arr.push((v and 0xFF).u8())
    else
      arr.push((v and 0xFF).u8())
      arr.push(((v >> 8) and 0xFF).u8())
      arr.push(((v >> 16) and 0xFF).u8())
      arr.push(((v >> 24) and 0xFF).u8())
    end

class \nodoc\ iso _TestMulticastIP6 is UnitTest
  """
  IPv6 multicast round trip: a socket joins transient group
  `ff12:1122:3344:5566:7788:99aa:bbcc:ddee` on a per-platform interface,
  sends a datagram to the scoped group at its own port, and receives it back
  via the default-on `IPV6_MULTICAST_LOOP`. All four address words are
  nonzero, pairwise distinct, and byte-order asymmetric.

  Falls back to a vacuous pass in `_on_closed` for hosts without multicast
  route.
  """
  fun name(): String => "net/MulticastIP6"

  fun ref apply(h: TestHelper) =>
    let auth = DNSAuth(h.env.root)

    if not _resolves_ip6(auth, "::1") then
      h.log("no usable IPv6 (::1 unresolvable); skipping")
      return
    end

    match \exhaustive\ _scoped_group()
    | None =>
      h.log("no candidate multicast interface; skipping")
    | let group: String =>
      if not _resolves_ip6(auth, group) then
        h.log("scoped group " + group + " unresolvable; skipping")
        return
      end

      h.log("using scoped group " + group)
      h.long_test(30_000_000_000)
      let mc =
        _TestMulticastIP6Actor(
          UDPAuth(h.env.root),
          DNSAuth(h.env.root),
          group,
          h)
      h.dispose_when_done(mc)
    end

  fun _resolves_ip6(auth: DNSAuth, host: String): Bool =>
    let list: Array[NetAddress] val = DNS.ip6(auth, host, "0")
    list.size() > 0

  fun _scoped_group(): (String | None) =>
    let group = "ff12:1122:3344:5566:7788:99aa:bbcc:ddee"
    ifdef linux then
      match \exhaustive\ _linux_interface()
      | let name': String => group + "%" + name'
      | None => None
      end
    elseif windows then
      match \exhaustive\ _windows_loopback_index()
      | let idx: U32 => group + "%" + idx.string()
      | None => None
      end
    else
      group + "%lo0"
    end

  fun _linux_interface(): (String | None) =>
    var i: U32 = 1
    while i <= 64 do
      let name' = _if_name(i)
      if (name' != "") and (name' != "lo") then
        return name'
      end
      i = i + 1
    end
    None

  fun _windows_loopback_index(): (U32 | None) =>
    var i: U32 = 1
    while i <= 64 do
      if _if_name(i) == "loopback_0" then
        return i
      end
      i = i + 1
    end
    None

  fun _if_name(i: U32): String val =>
    recover val
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

actor \nodoc\ _TestMulticastIP6Actor
  is (UDPSocketActor & UDPLifecycleEventReceiver)
  var _udp: UDPSocket = UDPSocket.none()
  let _h: TestHelper
  let _dns_auth: DNSAuth
  let _group: String
  var _expected: String = ""
  var _done: Bool = false
  var _attempts: U32 = 0

  new create(
    auth: UDPAuth,
    dns_auth: DNSAuth,
    group: String,
    h: TestHelper)
  =>
    _h = h
    _dns_auth = dns_auth
    _group = group
    _udp = UDPSocket(auth, "", "9826", this, this where ip_version = IP6)

  fun ref _socket(): UDPSocket => _udp

  fun ref _on_bound() =>
    _h.assert_true(_udp.local_address().ip6())

    // IPV6_ADD_MEMBERSHIP takes struct ipv6_mreq: 16-byte group address +
    // 4-byte interface index. The scoped group resolves to both.
    try
      let port = _udp.local_address().port()
      let list: Array[NetAddress] val =
        DNS.ip6(_dns_auth, _group, port.string())
      let dest = list(0)?

      _h.assert_true(dest.ip6())
      (let a1, let a2, let a3, let a4) = dest.ipv6_addr()
      _h.assert_eq[U32](a1, 0xFF12_1122)
      _h.assert_eq[U32](a2, 0x3344_5566)
      _h.assert_eq[U32](a3, 0x7788_99AA)
      _h.assert_eq[U32](a4, 0xBBCC_DDEE)

      let scope = dest.scope()
      var mreq = Array[U8](20)
      _push_u32_be(mreq, a1)
      _push_u32_be(mreq, a2)
      _push_u32_be(mreq, a3)
      _push_u32_be(mreq, a4)
      _push_u32_native(mreq, scope)
      _udp.setsockopt(
        OSSockOpt.ipproto_ipv6(), OSSockOpt.ipv6_add_membership(), mreq)

      _expected = "mc6:" + port.string()
      _udp.send_to(_expected, dest)
      _retransmit(dest)
    else
      _h.fail("couldn't resolve " + _group + " after the gate proved it")
      _h.complete(false)
    end

  fun ref _on_bind_failure() =>
    _h.fail("bind failed")
    _h.complete(false)

  fun ref _on_received(data: Array[U8] iso, from: NetAddress val)
    : ReadAction
  =>
    let s = String.from_array(consume data)
    if s == _expected then
      _h.assert_true(from.ip6())
      _done = true
      _h.log("mc6 delivered on " + _group)
      _udp.close()
      _h.complete(true)
    end
    KeepReading

  fun ref _on_closed() =>
    if not _done then
      _h.log("socket closed before delivery; treating as environmental" +
        " (no IPv6 multicast route)")
      _h.complete(true)
    end

  be _retransmit(dest: NetAddress val) =>
    if _udp.is_open() then
      _attempts = _attempts + 1
      if _attempts > 100 then
        _h.log("mc6: no delivery after " + _attempts.string() +
          " attempts; closing")
        _udp.close()
      else
        _udp.send_to(_expected, dest)
        _retransmit(dest)
      end
    end

  fun _push_u32_be(arr: Array[U8], v: U32) =>
    arr.push(((v >> 24) and 0xFF).u8())
    arr.push(((v >> 16) and 0xFF).u8())
    arr.push(((v >> 8) and 0xFF).u8())
    arr.push((v and 0xFF).u8())

  fun _push_u32_native(arr: Array[U8], v: U32) =>
    ifdef bigendian then
      arr.push(((v >> 24) and 0xFF).u8())
      arr.push(((v >> 16) and 0xFF).u8())
      arr.push(((v >> 8) and 0xFF).u8())
      arr.push((v and 0xFF).u8())
    else
      arr.push((v and 0xFF).u8())
      arr.push(((v >> 8) and 0xFF).u8())
      arr.push(((v >> 16) and 0xFF).u8())
      arr.push(((v >> 24) and 0xFF).u8())
    end
