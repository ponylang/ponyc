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
    _resolve(1, host, service)

  fun ip6(host: String, service: String): Array[IPAddress] iso^ =>
    """
    Gets all IPv6 addresses for a host and service.
    """
    _resolve(2, host, service)

  fun broadcast_ip4(service: String): Array[IPAddress] iso^ =>
    """
    Link-local IP4 broadcast address.
    """
    ip4("255.255.255.255", service)

  fun broadcast_ip6(service: String): Array[IPAddress] iso^ =>
    """
    Link-local IP6 broadcast address.
    """
    ip6("FF02::1", service)

  fun is_ip4(host: String): Bool =>
    """
    Returns true if the host is a literal IPv4 address.
    """
    @pony_os_host_ip4[Bool](host.cstring())

  fun is_ip6(host: String): Bool =>
    """
    Returns true if the host is a literal IPv6 address.
    """
    @pony_os_host_ip6[Bool](host.cstring())

  fun _resolve(family: U32, host: String, service: String):
    Array[IPAddress] iso^
  =>
    """
    Turns an addrinfo pointer into an array of addresses.
    """
    var list = recover Array[IPAddress] end
    var result = @pony_os_addrinfo[Pointer[U8]](
      family, host.cstring(), service.cstring())

    if not result.is_null() then
      var addr = result

      while not addr.is_null() do
        let ip = recover IPAddress end
        @pony_os_getaddr[None](addr, ip)
        list.push(consume ip)
        addr = @pony_os_nextaddr[Pointer[U8]](addr)
      end

      @freeaddrinfo[None](result)
    end

    list
