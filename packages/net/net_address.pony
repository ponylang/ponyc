use @ntohl[U32](netlong: U32)
use @ntohs[U16](netshort: U16)
use @pony_os_ipv4[Bool](addr: NetAddress tag)
use @pony_os_ipv6[Bool](addr: NetAddress tag)
use @ponyint_address_length[U32](addr: NetAddress tag)
use @pony_os_nameinfo[Bool](addr: NetAddress tag,
  host: Pointer[Pointer[U8] iso] tag, serv: Pointer[Pointer[U8] iso] tag,
  reverse_dns: Bool, service_name: Bool)

class val NetAddress is Equatable[NetAddress]
  """
  An IPv4 or IPv6 socket address, modelled after `sockaddr_storage`. The
  `_family` field indicates which kind; the `_addr` field is either the IPv4
  address or the IPv6 flow info, and `_addr1`–`_addr4` are the IPv6 address
  (invalid for IPv4). All address and port fields are in network byte order;
  `_scope` is in host byte order.

  Call `name()` to get the address and port as strings.
  """
  let _family: U16 = 0
  let _port: U16 = 0
    """
    Port number in network byte order.
    """
  let _addr: U32 = 0
    """
    IPv4 address in network byte order, or `0` for IPv6.
    """
  let _addr1: U32 = 0
    """
    Bits 0–32 of the IPv6 address in network byte order. `0` for IPv4.
    """
  let _addr2: U32 = 0
    """
    Bits 33–64 of the IPv6 address in network byte order. `0` for IPv4.
    """
  let _addr3: U32 = 0
    """
    Bits 65–96 of the IPv6 address in network byte order. `0` for IPv4.
    """
  let _addr4: U32 = 0
    """
    Bits 97–128 of the IPv6 address in network byte order. `0` for IPv4.
    """
  let _scope: U32 = 0
    """
    IPv6 scope zone identifier (`sin6_scope_id`) in host byte order.
    For link-local and scoped multicast addresses, the interface index that
    scopes the address. `0` for global addresses; invalid for IPv4.
    """

  fun ip4(): Bool =>
    """
    `true` when this is an IPv4 address.
    """
    @pony_os_ipv4(this)

  fun ip6(): Bool =>
    """
    `true` when this is an IPv6 address.
    """
    @pony_os_ipv6(this)

  fun name(
    reversedns: (DNSAuth | None) = None,
    servicename: Bool = false)
    : (String, String) ?
  =>
    """
    The host and service as strings. When `reversedns` is a `DNSAuth`, a DNS
    lookup returns the hostname; when it is `None` the plain IP address is
    returned. When `servicename` is `true` the port is translated to its
    service name (e.g. port 80 becomes `"http"`). Raises an error when reverse
    DNS is requested and no hostname is found. Uses `getnameinfo` internally.
    """
    var host: Pointer[U8] iso = recover Pointer[U8] end
    var serv: Pointer[U8] iso = recover Pointer[U8] end
    let reverse = reversedns isnt None

    if not
      @pony_os_nameinfo(
        this,
        addressof host,
        addressof serv,
        reverse,
        servicename)
    then
      error
    end

    (recover String.from_cstring(consume host) end,
      recover String.from_cstring(consume serv) end)

  fun eq(that: NetAddress box): Bool =>
    """
    Two addresses are equal when they have the same family, port, host, and
    scope.
    """
    (this._family == that._family) and
      (this._port == that._port) and
      (host_eq(that)) and
      (this._scope == that._scope)

  fun host_eq(that: NetAddress box): Bool =>
    """
    True when the host portion matches, ignoring port and scope.
    """
    if ip4() then
      this._addr == that._addr
    else
      (this._addr1 == that._addr1) and
        (this._addr2 == that._addr2) and
        (this._addr3 == that._addr3) and
        (this._addr4 == that._addr4)
    end

  fun length() : U8 =>
    """
    On platforms whose `sockaddr` includes a length field (macOS, FreeBSD),
    returns that length. On Linux and Windows, returns the size of
    `sockaddr_in` or `sockaddr_in6`.
    """

    ifdef linux or windows then
      (@ponyint_address_length(this)).u8()
    else
      ifdef bigendian then
        ((_family >> 8) and 0xff).u8()
      else
        (_family and 0xff).u8()
      end
    end

  fun family() : U8 =>
    """
    The address family.
    """

    ifdef linux or windows then
      ifdef bigendian then
        ((_family >> 8) and 0xff).u8()
      else
        (_family and 0xff).u8()
      end
    else
      ifdef bigendian then
        (_family and 0xff).u8()
      else
        ((_family >> 8) and 0xff).u8()
      end
    end

  fun port() : U16 =>
    """
    Port number in host byte order.
    """
    @ntohs(_port)

  fun scope() : U32 =>
    """
    IPv6 scope zone identifier (`sin6_scope_id`). For link-local and scoped
    multicast addresses, the interface index (e.g. `fe80::1%eth0`). `0` for
    global addresses; invalid for IPv4.
    """
    // The kernel and getaddrinfo keep sin6_scope_id in host byte order,
    // so no ntohl here.
    _scope

  fun ipv4_addr() : U32 =>
    """
    IPv4 address in host byte order. Valid only when `ip4()` is `true`.
    """
    @ntohl(_addr)

  fun ipv6_addr() : (U32, U32, U32, U32) =>
    """
    IPv6 address as four 32-bit words in host byte order. Valid only when
    `ip6()` is `true`.
    """
    ( @ntohl(_addr1),
      @ntohl(_addr2),
      @ntohl(_addr3),
      @ntohl(_addr4) )
