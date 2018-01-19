
primitive OSSocket
  """
  TODO
  """

  fun option_size_changed_error(): I32 =>
    """
    Error return value when the `getsockopt(2)` system call was
    given an `option_size` of `N` bytes, but the `option_size` value
    after a successful system call is `M` bytes instead
    (i.e., `N != M`).
    """
    -500

  fun get_so_error(fd: U32): I32 =>
    _get_so(fd, OSSockOpt.sol_socket(), OSSockOpt.so_error())

  fun get_so_rcvbuf(fd: U32): I32 =>
    _get_so(fd, OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf())

  fun get_so_sndbuf(fd: U32): I32 =>
    _get_so(fd, OSSockOpt.sol_socket(), OSSockOpt.so_sndbuf())

  fun get_tcp_nodelay(fd: U32): I32 =>
    _get_so(fd, OSSockOpt.sol_socket(), OSSockOpt.tcp_nodelay())

  fun set_so_broadcast(fd: U32, state: Bool): I32 =>
    _set_so(fd, OSSockOpt.sol_socket(), OSSockOpt.so_broadcast(),
      if state then 1 else 0 end)

  fun set_ip_multicast_loop(fd: U32, loopback: Bool): I32 =>
    _set_so(fd, OSSockOpt.sol_socket(), OSSockOpt.ip_multicast_loop(),
      if loopback then 1 else 0 end)

  fun set_ip_multicast_ttl(fd: U32, ttl: U8): I32 =>
    _set_so(fd, OSSockOpt.sol_socket(), OSSockOpt.ip_multicast_ttl(), ttl.i32())

  fun set_so_rcvbuf(fd: U32, bufsize: I32): I32 =>
    _set_so(fd, OSSockOpt.sol_socket(), OSSockOpt.so_rcvbuf(), bufsize)

  fun set_so_sndbuf(fd: U32, bufsize: I32): I32 =>
    _set_so(fd, OSSockOpt.sol_socket(), OSSockOpt.so_sndbuf(), bufsize)

  fun set_tcp_nodelay(fd: U32, state: Bool): I32 =>
    _set_so(fd, OSSockOpt.sol_socket(), OSSockOpt.tcp_nodelay(),
      if state then 1 else 0 end)


  fun _get_so(fd: U32, level: I32, option_name: I32): I32 =>
    var option: I32 = -1
    var option_size: I32 = 4

    let res: I32 = @pony_os_getsockopt[I32](fd, level, option_name,
      addressof option, addressof option_size)
    if res == 0 then
      if option_size == 4 then
        option
      else
        option_size_changed_error()
      end
    else
      res
    end

  fun _set_so(fd: U32, level: I32, option_name: I32, option': I32): I32 =>
    var option: I32 = option'
    @pony_os_setsockopt[I32](fd, level, option_name, addressof option, I32(4))
