class IPAddress val
  """
  Represents an IPv4 or IPv6 address. The family field indicates the address
  type. The addr field is either the IPv4 address or the IPv6 flow info. The
  addr1-4 fields are the IPv6 address, or invalid for an IPv4 address. The
  scope field is the IPv6 scope, or invalid for an IPv4 address.
  """
  let length: U8 = 0
  let family: U8 = 0
  let port: U16 = 0
  let addr: U32 = 0
  let addr1: U32 = 0
  let addr2: U32 = 0
  let addr3: U32 = 0
  let addr4: U32 = 0
  let scope: U32 = 0

  fun ip4(): Bool =>
    """
    Returns true for an IPv4 address.
    """
    @os_ipv4[Bool](this)

  fun ip6(): Bool =>
    """
    Returns true for an IPv6 address.
    """
    @os_ipv6[Bool](this)

  fun name(reversedns: Bool = false, servicename: Bool = false):
    (String, String) ?
  =>
    """
    Return the host and service name.
    """
    let host: Pointer[U8] iso = recover Pointer[U8] end
    let serv: Pointer[U8] iso = recover Pointer[U8] end
    @os_nameinfo[None](this, &host, &serv, reversedns, servicename) ?

    (recover String.from_cstring(consume host) end,
      recover String.from_cstring(consume serv) end)
