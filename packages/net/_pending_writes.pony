class _PendingWrites
  """
  The buffers waiting to go on the wire, and how far into the first one the last
  partial write reached.

  `_first_offset` points into the buffer `_buffers(0)` owns, so trimming the
  buffers and resetting the offset have to happen together, or the offset
  dangles. `_total` is the byte count those two imply. This type is the only
  place all three change, so they cannot drift: `push`, `sent`, and `clear` are
  the only mutations, and each keeps them consistent.
  """
  embed _buffers: Array[ByteSeq] = _buffers.create()
  var _first_offset: USize = 0
  var _total: USize = 0

  fun size(): USize =>
    """
    Number of queued buffers.
    """
    _buffers.size()

  fun total(): USize =>
    """
    Bytes still to send, counting the offset into the first buffer.
    """
    _total

  fun first_offset(): USize =>
    """
    Bytes of the first buffer already sent.
    """
    _first_offset

  fun prefix_total(n: USize): USize =>
    """
    Bytes in the first `n` buffers, counting the offset already sent from the
    first. `n >= size()` is `total()`. The byte count for a capped sendv batch.
    """
    if n >= _buffers.size() then return _total end
    var sum: USize = 0
    var i: USize = 0
    while i < n do
      try
        let s = _buffers(i)?.size()
        sum = sum + if i == 0 then s - _first_offset else s end
      else
        _Unreachable()
      end
      i = i + 1
    end
    sum

  fun buffers(): this->Array[ByteSeq] =>
    """
    The buffers, for `RuntimeBackend.sendv`. Read together with `first_offset`.
    """
    _buffers

  fun ref push(buffer: ByteSeq) =>
    _buffers.push(buffer)
    _total = _total + buffer.size()

  fun ref clear() =>
    _buffers.clear()
    _first_offset = 0
    _total = 0

  fun ref sent(bytes: USize) =>
    """
    Account for `bytes` written from the head of the queue: trim the fully-sent
    buffers, advance the offset into the new first buffer, and lower the total.
    """
    if bytes == 0 then return end

    var remaining = bytes
    var num_fully_sent: USize = 0
    var new_offset: USize = 0

    while remaining > 0 do
      try
        let entry = _buffers(num_fully_sent)?
        let start = if num_fully_sent == 0 then _first_offset else USize(0) end
        let effective_size = entry.size() - start

        if effective_size <= remaining then
          num_fully_sent = num_fully_sent + 1
          remaining = remaining - effective_size
        else
          new_offset = start + remaining
          remaining = 0
        end
      else
        _Unreachable()
      end
    end

    _buffers.trim_in_place(num_fully_sent)
    _first_offset = new_offset
    _total = _total - bytes
