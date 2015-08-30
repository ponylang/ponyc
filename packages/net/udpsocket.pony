use "collections"

actor UDPSocket
  var _notify: UDPNotify
  var _fd: U64
  var _event: AsioEventID
  var _readable: Bool = false
  var _closed: Bool = false
  var _packet_size: U64
  var _read_buf: Array[U8] iso = recover Array[U8].undefined(64) end
  var _read_from: IPAddress iso = recover IPAddress end

  new create(root: Root, notify: UDPNotify iso,
    host: String = "", service: String = "0",
    size: U64 = 1024)
  =>
    """
    Listens for both IPv4 and IPv6 datagrams.
    """
    _notify = consume notify
    _event = @os_listen_udp[AsioEventID](this, host.cstring(), service.cstring())
    _fd = @asio_event_data[U64](_event)
    _packet_size = size
    _notify_listening()
    _start_next_read()

  new ip4(root: Root, notify: UDPNotify iso,
    host: String = "", service: String = "0",
    size: U64 = 1024)
  =>
    """
    Listens for IPv4 datagrams.
    """
    _notify = consume notify
    _event = @os_listen_udp4[AsioEventID](this, host.cstring(), service.cstring())
    _fd = @asio_event_data[U64](_event)
    _packet_size = size
    _notify_listening()
    _start_next_read()

  new ip6(root: Root, notify: UDPNotify iso,
    host: String = "", service: String = "0",
    size: U64 = 1024)
  =>
    """
    Listens for IPv6 datagrams.
    """
    _notify = consume notify
    _event = @os_listen_udp6[AsioEventID](this, host.cstring(), service.cstring())
    _fd = @asio_event_data[U64](_event)
    _packet_size = size
    _notify_listening()
    _start_next_read()

  be write(data: Bytes, to: IPAddress) =>
    """
    Write a single sequence of bytes.
    """
    _write(data, to)

  be writev(data: BytesList val, to: IPAddress) =>
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

  be set_multicast_interface(from: String = "") =>
    """
    By default, the OS will choose which address is used to send packets bound
    for multicast addresses. This can be used to force a specific interface. To
    revert to allowing the OS to choose, call with an empty string.
    """
    @os_multicast_interface[None](_fd, from.cstring())

  be set_multicast_loopback(loopback: Bool) =>
    """
    By default, packets sent to a multicast address will be received by the
    sending system if it has subscribed to that address. Disabling loopback
    prevents this.
    """
    @os_multicast_loopback[None](_fd, loopback)

  be set_multicast_ttl(ttl: U8) =>
    """
    Set the TTL for multicast sends. Defaults to 1.
    """
    @os_multicast_ttl[None](_fd, ttl)

  be multicast_join(group: String, to: String = "") =>
    """
    Add a multicast group. This can be limited to packets arriving on a
    specific interface.
    """
    @os_multicast_join[None](_fd, group.cstring(), to.cstring())

  be multicast_leave(group: String, to: String = "") =>
    """
    Drop a multicast group. This can be limited to packets arriving on a
    specific interface. No attempt is made to check that this socket has
    previously added this group.
    """
    @os_multicast_leave[None](_fd, group.cstring(), to.cstring())

  be dispose() =>
    """
    Stop listening.
    """
    _close()

  fun local_address(): IPAddress =>
    """
    Return the bound IP address.
    """
    let ip = recover IPAddress end
    @os_sockname[Bool](_fd, ip)
    ip

  be _event_notify(event: AsioEventID, flags: U32, arg: U64) =>
    """
    When we are readable, we accept new connections until none remain.
    """
    if event isnt _event then
      return
    end

    if not _closed then
      _event = event

      if AsioEvent.readable(flags) then
        _readable = true
        _complete_reads(arg)
        _pending_reads()
      end
    elseif Platform.windows() and AsioEvent.readable(flags) then
      _readable = false
      _close()
    end

    if AsioEvent.disposable(flags) then
      @asio_event_destroy[None](_event)
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
    if not Platform.windows() then
      try
        var sum: U64 = 0

        while _readable do
          var len = _packet_size
          var data = recover Array[U8].undefined(len) end
          var from = recover IPAddress end
          len = @os_recvfrom[U64](_event, data.cstring(), data.space(), from) ?

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

  fun ref _complete_reads(len: U64) =>
    """
    The OS has informed as that len bytes of pending reads have completed.
    This occurs only with IOCP on Windows.
    """
    if Platform.windows() then
      var next = _read_buf.space()

      match len
      | 0 =>  // Socket has been closed
        _readable = false
        _close()
        return
      | _read_buf.space() =>
        // Whole buffer was used this time, next time make it bigger
        next = next * 2
      end

      if _closed then
        return
      end

      // Hand back read data
      let data = _read_buf = recover Array[U8].undefined(next) end
      let from = _read_from = recover IPAddress end
      data.truncate(len)
      _notify.received(this, consume data, consume from)

      _start_next_read()
    end

  fun ref _start_next_read() =>
    """
    Start our next receive.
    This is used only with IOCP on Windows.
    """
    if Platform.windows() then
      try
        @os_recvfrom[U64](_event, _read_buf.cstring(), _read_buf.space(),
          _read_from) ?
      else
        _readable = false
        _close()
      end
    end

  fun ref _write(data: Bytes, to: IPAddress) =>
    """
    Write the datagram to the socket.
    """
    if not _closed then
      try
        @os_sendto[U64](_fd, data.cstring(), data.size(), to) ?
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
    if Platform.windows() then
      // On windows, wait until IOCP read operation has completed or been
      // cancelled.
      if not _readable then
        @asio_event_unsubscribe[None](_event)
      end
    else
      // Unsubscribe immediately.
      @asio_event_unsubscribe[None](_event)
      _readable = false
    end

    _closed = true

    if _fd != -1 then
      _notify.closed(this)
      // On windows, this will also cancel all outstanding IOCP operations.
      @os_closesocket[None](_fd)
      _fd = -1
    end
