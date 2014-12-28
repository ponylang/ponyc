trait Socket
  fun box local_address(): (String, String) ? =>
    """
    Return the local host and service name.
    """
    let (host, serv) =
      @os_sockname[(Pointer[U8] iso^, Pointer[U8] iso^)](_get_fd()) ?
    (recover String.from_cstring(consume host) end,
      recover String.from_cstring(consume serv) end)

  fun box remote_address(): (String, String) ? =>
    """
    Return the remote host and service name.
    """
    let (host, serv) =
      @os_peername[(Pointer[U8] iso^, Pointer[U8] iso^)](_get_fd()) ?
    (recover String.from_cstring(consume host) end,
      recover String.from_cstring(consume serv) end)

  fun box _get_fd(): U32
