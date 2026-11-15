class ref FrameDecoder[A: Any val]
  """
  Wraps any `Codec[A]` for use with TCP stream framing.

  Incoming `iso` chunks are concatenated with any unconsumed remainder into
  a `val` buffer for decoding. Decoded byte slices are zero-copy views into
  that buffer. After the decode loop, the unconsumed tail is kept as a
  zero-copy `trim` for the next call. A sub-byte bit offset is carried
  across calls so codecs that produce non-byte-aligned frames advance
  correctly.

  `InsufficientData` means "wait for more bytes" — normal in streaming.
  `MalformedData` means "this data is wrong" — reported as an error via
  `FrameNotify.frame_error`.
  """
  let _codec: Codec[A]
  var _remainder: Array[U8] val
  var _bit_offset: U8

  new ref create(codec: Codec[A]) =>
    _codec = codec
    _remainder = recover val Array[U8] end
    _bit_offset = 0

  fun ref received(data: Array[U8] iso, notify: FrameNotify[A] ref) =>
    """
    Append a chunk of incoming data and extract all complete messages.
    """
    let buf: Array[U8] val =
      if _remainder.size() == 0 then
        consume data
      else
        recover val
          Array[U8]
            .> append(_remainder)
            .> append(consume data)
        end
      end

    var bit_pos: USize = _bit_offset.usize()
    var continue' = true
    while continue' do
      match \exhaustive\ _codec.decode(buf, bit_pos)
      | (let value: A, let consumed: USize) =>
        notify.frame_received(value)
        bit_pos = bit_pos + consumed
      | InsufficientData => continue' = false
      | let m: MalformedData =>
        notify.frame_error(m)
        continue' = false
      end
    end
    _remainder = buf.trim(bit_pos / 8)
    _bit_offset = (bit_pos % 8).u8()
