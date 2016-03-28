use "collections"

type UDPSocketAuth is (AmbientAuth | NetAuth | UDPAuth)

actor UDPSocket
  var _notify: UDPNotify
  var _fd: U32
  var _event: AsioEventID
  var _readable: Bool = false
  var _closed: Bool = false
  var _packet_size: USize
  var _read_buf: Array[U8] iso
  var _read_from: IPAddress iso = IPAddress
  embed _ip: IPAddress = IPAddress

  new create(auth: UDPSocketAuth, notify: UDPNotify iso, host: String = "",
    service: String = "0", size: USize = 1024)
  =>
    """
    Listens for both IPv4 and IPv6 datagrams.
    """
    _notify = consume notify
    _event = @pony_os_listen_udp[AsioEventID](this, host.cstring(),
      service.cstring())
    _fd = @pony_asio_event_fd(_event)
    @pony_os_sockname[Bool](_fd, _ip)
    _packet_size = size
    _read_buf = recover Array[U8].undefined(size) end
    _notify_listening()
    _start_next_read()

  new ip4(auth: UDPSocketAuth, notify: UDPNotify iso, host: String = "",
    service: String = "0", size: USize = 1024)
  =>
    """
    Listens for IPv4 datagrams.
    """
    _notify = consume notify
    _event = @pony_os_listen_udp4[AsioEventID](this, host.cstring(),
      service.cstring())
    _fd = @pony_asio_event_fd(_event)
    @pony_os_sockname[Bool](_fd, _ip)
    _packet_size = size
    _read_buf = recover Array[U8].undefined(size) end
    _notify_listening()
    _start_next_read()

  new ip6(auth: UDPSocketAuth, notify: UDPNotify iso, host: String = "",
    service: String = "0", size: USize = 1024)
  =>
    """
    Listens for IPv6 datagrams.
    """
    _notify = consume notify
    _event = @pony_os_listen_udp6[AsioEventID](this, host.cstring(),
      service.cstring())
    _fd = @pony_asio_event_fd(_event)
    @pony_os_sockname[Bool](_fd, _ip)
    _packet_size = size
    _read_buf = recover Array[U8].undefined(size) end
    _notify_listening()
    _start_next_read()

  be write(data: ByteSeq, to: IPAddress) =>
    """
    Write a single sequence of bytes.
    """
    _write(data, to)

  be writev(data: ByteSeqIter, to: IPAddress) =>
    """
    Write a sequence of sequences of bytes.
    """
    for bytes in data.values() do
      _write(bytes, to)
    end

  be set_notify(notify: UDPNotify iso) =>
    """
    Change the notifier.
    """
    _notify = consume notify

  be set_broadcast(state: Bool) =>
    """
    Enable or disable broadcasting from this socket.
    """
    if _ip.ip4() then
      @pony_os_broadcast[None](_fd, state)
    elseif _ip.ip6() then
      @pony_os_multicast_join[None](_fd, "FF02::1".cstring(), "".cstring())
    end

  be set_multicast_interface(from: String = "") =>
    """
    By default, the OS will choose which address is used to send packets bound
    for multicast addresses. This can be used to force a specific interface. To
    revert to allowing the OS to choose, call with an empty string.
    """
    @pony_os_multicast_interface[None](_fd, from.cstring())

  be set_multicast_loopback(loopback: Bool) =>
    """
    By default, packets sent to a multicast address will be received by the
    sending system if it has subscribed to that address. Disabling loopback
    prevents this.
    """
    @pony_os_multicast_loopback[None](_fd, loopback)

  be set_multicast_ttl(ttl: U8) =>
    """
    Set the TTL for multicast sends. Defaults to 1.
    """
    @pony_os_multicast_ttl[None](_fd, ttl)

  be multicast_join(group: String, to: String = "") =>
    """
    Add a multicast group. This can be limited to packets arriving on a
    specific interface.
    """
    @pony_os_multicast_join[None](_fd, group.cstring(), to.cstring())

  be multicast_leave(group: String, to: String = "") =>
    """
    Drop a multicast group. This can be limited to packets arriving on a
    specific interface. No attempt is made to check that this socket has
    previously added this group.
    """
    @pony_os_multicast_leave[None](_fd, group.cstring(), to.cstring())

  be dispose() =>
    """
    Stop listening.
    """
    _close()

  fun local_address(): IPAddress =>
    """
    Return the bound IP address.
    """
    _ip

  be _event_notify(event: AsioEventID, flags: U32, arg: U32) =>
    """
    When we are readable, we accept new connections until none remain.
    """
    if event isnt _event then
      return
    end

    if not _closed then
      if AsioEvent.readable(flags) then
        _readable = true
        _complete_reads(arg)
        _pending_reads()
      end
    else
      ifdef windows then
        if AsioEvent.readable(flags) then
          _readable = false
          _close()
        end
      end
    end

    if AsioEvent.disposable(flags) then
      @pony_asio_event_destroy[None](_event)
      _event = AsioEvent.none()
    end

  be _read_again() =>
    """
    Resume reading.
    """
    if not _closed then
      _pending_reads()
    end

  fun ref _pending_reads() =>
    """
    Read while data is available, guessing the next packet length as we go. If
    we read 4 kb of data, send ourself a resume message and stop reading, to
    avoid starving other actors.
    """
    ifdef not windows then
      try
        var sum: USize = 0

        while _readable do
          let size = _packet_size
          let data = _read_buf = recover Array[U8].undefined(size) end
          let from = recover IPAddress end
          let len = @pony_os_recvfrom[USize](_event, data.cstring(),
            data.space(), from) ?

          if len == 0 then
            _readable = false
            return
          end

          data.truncate(len)
          _notify.received(this, consume data, consume from)

          sum = sum + len

          if sum > (1 << 12) then
            _read_again()
            return
          end
        end
      else
        _close()
      end
    end

  fun ref _complete_reads(len: U32) =>
    """
    The OS has informed as that len bytes of pending reads have completed.
    This occurs only with IOCP on Windows.
    """
    ifdef windows then
      if _read_buf.space() == 0 then
        // Socket has been closed
        _readable = false
        _close()
        return
      end

      if _closed then
        return
      end

      // Hand back read data
      let size = _packet_size
      let data = _read_buf = recover Array[U8].undefined(size) end
      let from = _read_from = recover IPAddress end
      data.truncate(len.usize())
      _notify.received(this, consume data, consume from)

      _start_next_read()
    end

  fun ref _start_next_read() =>
    """
    Start our next receive.
    This is used only with IOCP on Windows.
    """
    ifdef windows then
      try
        @pony_os_recvfrom[USize](_event, _read_buf.cstring(),
          _read_buf.space(), _read_from) ?
      else
        _readable = false
        _close()
      end
    end

  fun ref _write(data: ByteSeq, to: IPAddress) =>
    """
    Write the datagram to the socket.
    """
    if not _closed then
      try
        @pony_os_sendto[USize](_fd, data.cstring(), data.size(), to) ?
      else
        _close()
      end
    end

  fun ref _notify_listening() =>
    """
    Inform the notifier that we're listening.
    """
    if _fd != -1 then
      _notify.listening(this)
    else
      _notify.not_listening(this)
    end

  fun ref _close() =>
    """
    Inform the notifier that we've closed.
    """
    ifdef windows then
      // On windows, wait until IOCP read operation has completed or been
      // cancelled.
      if _closed and not _readable and not _event.is_null() then
        @pony_asio_event_unsubscribe[None](_event)
      end
    else
      // Unsubscribe immediately.
      if not _event.is_null() then
        @pony_asio_event_unsubscribe[None](_event)
        _readable = false
      end
    end

    _closed = true

    if _fd != -1 then
      _notify.closed(this)
      // On windows, this will also cancel all outstanding IOCP operations.
      @pony_os_socket_close[None](_fd)
      _fd = -1
    end
