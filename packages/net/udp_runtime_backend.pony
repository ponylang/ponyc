use @pony_os_listen_udp[AsioEventID](the_actor: AsioEventNotify,
  host: Pointer[U8] tag,
  port: Pointer[U8] tag)
use @pony_os_listen_udp4[AsioEventID](the_actor: AsioEventNotify,
  host: Pointer[U8] tag,
  port: Pointer[U8] tag)
use @pony_os_listen_udp6[AsioEventID](the_actor: AsioEventNotify,
  host: Pointer[U8] tag,
  port: Pointer[U8] tag)
use @pony_os_recvfrom[U8](event: AsioEventID,
  buffer: Pointer[U8] tag,
  size: USize,
  from: NetAddress tag,
  count_out: Pointer[USize])
use @pony_os_sendto[U8](fd: U32,
  data: Pointer[U8] tag,
  size: USize,
  to: NetAddress box,
  count_out: Pointer[USize])

class UDPRuntimeBackend is UDPBackend
  """
  Wrappers for the runtime's `pony_os_*` UDP functions -- bind, recvfrom,
  sendto, and socket teardown.
  """
  new create() => None

  fun ref bind(the_actor: AsioEventNotify,
    host: String,
    port: String,
    ip_version: IPVersion = DualStack)
    : AsioEventID
  =>
    match \exhaustive\ ip_version
    | IP4 =>
      @pony_os_listen_udp4(the_actor, host.cstring(), port.cstring())
    | IP6 =>
      @pony_os_listen_udp6(the_actor, host.cstring(), port.cstring())
    | DualStack =>
      @pony_os_listen_udp(the_actor, host.cstring(), port.cstring())
    end

  fun ref close(fd: U32) =>
    @pony_os_socket_close(fd)

  fun ref recvfrom(event: AsioEventID,
    buffer: Pointer[U8] tag,
    size: USize)
    : (SocketResult, USize, NetAddress iso^)
  =>
    """
    Read one datagram via `@pony_os_recvfrom`.
    """
    var count: USize = 0
    let from = recover iso NetAddress end
    let result =
      SocketResultDecoder(
        @pony_os_recvfrom(event, buffer, size, from, addressof count))
    (result, count, consume from)

  fun ref sendto(fd: U32,
    data: ByteSeq,
    to: NetAddress box)
    : SocketResult
  =>
    var count: USize = 0
    SocketResultDecoder(
      @pony_os_sendto(fd, data.cpointer(), data.size(), to, addressof count))

  fun ref sockname(fd: U32, ip: NetAddress tag): Bool =>
    @pony_os_sockname(fd, ip)
