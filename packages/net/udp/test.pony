use "net/dns"
use "net/ip"
use "ponytest"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestBroadcast)

class _TestPing is UDPNotify
  let _h: TestHelper
  let _ip: IPAddress

  new create(h: TestHelper, ip: IPAddress) =>
    _h = h

    _ip = try
      let auth = h.env.root as AmbientAuth
      (_, let service) = ip.name()

      let list = if ip.ip4() then
        ifdef freebsd then
          DNS.ip4(auth, "", service)
        else
          DNS.broadcast_ip4(auth, service)
        end
      else
        ifdef freebsd then
          DNS.ip6(auth, "", service)
        else
          DNS.broadcast_ip6(auth, service)
        end
      end

      list(0)
    else
      _h.fail("Couldn't make broadcast address")
      ip
    end

  fun ref not_listening(sock: UDPSocket ref) =>
    _h.fail_action("ping listen")

  fun ref listening(sock: UDPSocket ref) =>
    _h.complete_action("ping listen")

    sock.set_broadcast(true)
    sock.write("ping!", _ip)

  fun ref received(sock: UDPSocket ref, data: Array[U8] iso, from: IPAddress) =>
    _h.complete_action("ping receive")

    let s = String.append(consume data)
    _h.assert_eq[String box](s, "pong!")
    _h.complete(true)

class _TestPong is UDPNotify
  let _h: TestHelper

  new create(h: TestHelper) =>
    _h = h

  fun ref not_listening(sock: UDPSocket ref) =>
    _h.fail_action("pong listen")

  fun ref listening(sock: UDPSocket ref) =>
    _h.complete_action("pong listen")

    sock.set_broadcast(true)
    let ip = sock.local_address()

    try
      let auth = _h.env.root as AmbientAuth
      let h = _h
      if ip.ip4() then
        _h.dispose_when_done(
          UDPSocket.ip4(auth, recover _TestPing(h, ip) end))
      else
        _h.dispose_when_done(
          UDPSocket.ip6(auth, recover _TestPing(h, ip) end))
      end
    else
      _h.fail_action("ping create")
    end

  fun ref received(sock: UDPSocket ref, data: Array[U8] iso, from: IPAddress)
  =>
    _h.complete_action("pong receive")

    let s = String.append(consume data)
    _h.assert_eq[String box](s, "ping!")
    sock.writev(
      recover val [[U8('p'), U8('o'), U8('n'), U8('g'), U8('!')]] end,
      from)

class iso _TestBroadcast is UnitTest
  """
  Test broadcasting with UDP.
  """
  fun name(): String => "net/Broadcast"

  fun ref apply(h: TestHelper) =>
    h.expect_action("pong create")
    h.expect_action("pong listen")
    h.expect_action("ping create")
    h.expect_action("ping listen")
    h.expect_action("pong receive")
    h.expect_action("ping receive")

    try
      let auth = h.env.root as AmbientAuth
      h.dispose_when_done(UDPSocket(auth, recover _TestPong(h) end))
    else
      h.fail_action("pong create")
    end

    h.long_test(2_000_000_000) // 2 second timeout

