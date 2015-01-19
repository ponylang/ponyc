primitive DNS
  """
  Helper functions for resolving DNS queries.
  """
  fun apply(host: String, service: String): Array[IPAddress] iso^ =>
    """
    Gets all IPv4 and IPv6 addresses for a host and service.
    """
    _resolve(0, host, service)

  fun ip4(host: String, service: String): Array[IPAddress] iso^ =>
    """
    Gets all IPv4 addresses for a host and service.
    """
    _resolve(2, host, service)

  fun ip6(host: String, service: String): Array[IPAddress] iso^ =>
    """
    Gets all IPv6 addresses for a host and service.
    """
    _resolve(10, host, service)

  fun _resolve(family: U32, host: String, service: String):
    Array[IPAddress] iso^
  =>
    """
    Turns an addrinfo pointer into an array of addresses.
    """
    var list = recover Array[IPAddress] end
    var result = @os_addrinfo[Pointer[_AddrInfo]](
      family, host.cstring(), service.cstring())

    if not result.is_null() then
      var addr = result

      while not addr.is_null() do
        let ip = recover IPAddress end
        @os_getaddr[None](addr, ip)
        list.append(consume ip)
        addr = @os_nextaddr[Pointer[_AddrInfo]](addr)
      end

      @freeaddrinfo[None](result)
    end

    list

primitive _AddrInfo
