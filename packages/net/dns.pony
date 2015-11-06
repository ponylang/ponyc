primitive DNS
  """
  Helper functions for resolving DNS queries.
  """
  fun apply(root: AmbientAuth, host: String, service: String): Array[IPAddress] iso^ =>
    """
    Gets all IPv4 and IPv6 addresses for a host and service.
    """
    _resolve(0, host, service, root)

  fun ip4(root: AmbientAuth, host: String, service: String): Array[IPAddress] iso^ =>
    """
    Gets all IPv4 addresses for a host and service.
    """
    _resolve(2, host, service, root)

  fun ip6(root: AmbientAuth, host: String, service: String): Array[IPAddress] iso^ =>
    """
    Gets all IPv6 addresses for a host and service.
    """
    _resolve(10, host, service, root)

  fun is_ip4(host: String): Bool =>
    """
    Returns true if the host is a literal IPv4 address.
    """
    @os_host_ip4[Bool](host.cstring())

  fun is_ip6(host: String): Bool =>
    """
    Returns true if the host is a literal IPv6 address.
    """
    @os_host_ip6[Bool](host.cstring())

  fun _resolve(family: U32, host: String, service: String, root: AmbientAuth):
    Array[IPAddress] iso^
  =>
    """
    Turns an addrinfo pointer into an array of addresses.
    """
    var list = recover Array[IPAddress] end
    var result = @os_addrinfo[Pointer[U8]](
      family, host.cstring(), service.cstring())

    if not result.is_null() then
      var addr = result

      while not addr.is_null() do
        let ip = recover IPAddress end
        @os_getaddr[None](addr, ip)
        list.push(consume ip)
        addr = @os_nextaddr[Pointer[U8]](addr)
      end

      @freeaddrinfo[None](result)
    end

    list
