class val NetAddress is Equatable[NetAddress]
  """
  Represents an IPv4 or IPv6 address. The family field indicates the address
  type. The addr field is either the IPv4 address or the IPv6 flow info. The
  addr1-4 fields are the IPv6 address, or invalid for an IPv4 address. The
  scope field is the IPv6 scope, or invalid for an IPv4 address.

  This class is modelled after the C data structure for holding socket
  addresses for both IPv4 and IPv6 `sockaddr_storage`.

  Use the `name` method to obtain address/hostname and port/service as Strings.
  """
  let _family: U16 = 0

  let _port: U16 = 0
    """
    Port number in network byte order.

    In order to obtain a host byte order port, use:

    ```pony
    let host_order_port: U16 = @ntohs[U16](net_address.port)
    ```
    """
  let _addr: U32 = 0
    """
    IPv4 address in network byte order.
    Will be `0` for IPv6 addresses. Check with `ipv4()` and `ipv6()`.

    Use `@ntohl[U32](net_address.addr)` to obtain it in the host byte order.
    """

  let _addr1: U32 = 0
    """
    Bits 0-32 of the IPv6 address in network byte order.

    `0` if this is an IPv4 address. Check with `ipv4()` and `ipv6()`.
    """

  let _addr2: U32 = 0
    """
    Bits 33-64 of the IPv6 address in network byte order.

    `0` if this is an IPv4 address. Check with `ipv4()` and `ipv6()`.
    """
  let _addr3: U32 = 0
    """
    Bits 65-96 of the IPv6 address in network byte order.

    `0` if this is an IPv4 address. Check with `ipv4()` and `ipv6()`.
    """
  let _addr4: U32 = 0
    """
    Bits 97-128 of the IPv6 address in network byte order.

    `0` if this is an IPv4 address. Check with `ipv4()` and `ipv6()`.
    """

  let _scope: U32 = 0
    """IPv6 scope identifier: Unicast, Anycast, Multicast and unassigned scopes."""

  fun ip4(): Bool =>
    """
    Returns true for an IPv4 address.
    """
    @pony_os_ipv4[Bool](this)

  fun ip6(): Bool =>
    """
    Returns true for an IPv6 address.
    """
    @pony_os_ipv6[Bool](this)

  fun name(
    reversedns: (DNSLookupAuth | None) = None,
    servicename: Bool = false)
    : (String, String) ?
  =>
    """
    Returns the host and service name.

    If `reversedns` is an instance of `DNSLookupAuth`
    a DNS lookup will be executed and the hostname
    for this address is returned as first element of the result tuple.
    If no hostname could be found, an error is raised.
    If `reversedns` is `None` the plain IP address is given
    and no DNS lookup is executed.

    If `servicename` is `false` the numeric port is returned
    as second element of the result tuple.
    If it is `true` the port is translated into its
    corresponding servicename (e.g. port 80 is returned as `"http"`).

    Internally this method uses the POSIX C function `getnameinfo`.
    """
    var host: Pointer[U8] iso = recover Pointer[U8] end
    var serv: Pointer[U8] iso = recover Pointer[U8] end
    let reverse = reversedns isnt None

    if not
      @pony_os_nameinfo[Bool](this, addressof host, addressof serv, reverse,
        servicename)
    then
      error
    end

    (recover String.from_cstring(consume host) end,
      recover String.from_cstring(consume serv) end)

  fun eq(that: NetAddress box): Bool =>
      (this._family == that._family)
      and (this._port == that._port)
      and (host_eq(that))
      and (this._scope == that._scope)

  fun host_eq(that: NetAddress box): Bool =>
    if ip4() then
      this._addr == that._addr
    else
      (this._addr1 == that._addr1)
        and (this._addr2 == that._addr2)
        and (this._addr3 == that._addr3)
        and (this._addr4 == that._addr4)
    end

  fun length() : U8 =>
    """
    For platforms (OSX/FreeBSD) with `length` field as part of 
    its `struct sockaddr` definition, returns the `length`. 
    Else (Linux/Windows) returns the size of `sockaddr_in` or `sockaddr_in6`.
    """

    ifdef linux or windows then
      (@ponyint_address_length[U32](this)).u8()
    else
      ifdef bigendian then
        ((_family >> 8) and 0xff).u8()
      else
        (_family and 0xff).u8()
      end
    end

    fun family() : U8 =>
    """
      Returns the `family`.
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
      _port

    fun scope() : U32 =>
      _scope

    fun addr() : (U32 | Array[U32]) =>
      """
        Returns IPV4 address (`_addr` field in the class) if `ip4()` is `True`.
        If this is a IPV6 address, then returns an `Array` (say a), 
        such that 
              `a.size() = 4` 
              `a(0) = _addr1` // Bits 0-32 of the IPv6 address in network byte order.
              `a(1) = _addr2` // Bits 33-64 of the IPv6 address in network byte order.
              `a(2) = _addr3` // Bits 65-96 of the IPv6 address in network byte order.
              `a(3) = _addr4` // Bits 97-128 of the IPv6 address in network byte order.
      """
      if ip4() then
        _addr
      else
        [_addr1; _addr2; _addr3; _addr4]
      end
