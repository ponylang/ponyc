use "collections"

class WriteBuffer
  """
  * A List[ByteSeq] maintained as WriteBuffer.
  * _current keeps track of the current packet `ByteSeq iso`
    into which the user is writing
  * _buffer is returned on take_buffer() as a ByteSeqIter
  * The current packet is written into _buffer on call the new_packet()
  """
  var _buffer: List[ByteSeq] iso
  var _current: Array[U8] iso
  var _current_size: USize
  var _little_endian: Bool

  new create(little_endian: Bool = false) =>
    """
    Create the buffer
    Create _current depending on the seq_type requested by the user
    """
    _buffer = recover List[ByteSeq] end
    _current = recover Array[U8] end
    _current_size = 0
    _little_endian = little_endian

  fun ref new_packet(): WriteBuffer =>
    """
    * Archives the current packet into _buffer and ensures future
      writes goto a new packet
    * Destructive read of _current and push it onto buffer.
    * This is chainable
    """
    _buffer.push(_current = recover Array[U8] end)
    _current_size = 0
    this

  fun ref take_buffer(): ByteSeqIter =>
    """
    Destructive read of existing _buffer and return as ByteSeqIter
    """
    if (_current_size > 0) then
      new_packet()
    end
    _buffer = recover List[ByteSeq] end

  fun current_size(): USize =>
    """
    Size of current packet
    """
    _current_size

  fun ref _byte(value: U8 val): WriteBuffer =>
    """
    Endless writes into _current
    Chainable
    """
    _current.push(value)
    _current_size = _current_size + 1
    this

  fun ref add_byte_seq(bytes: ByteSeq): WriteBuffer =>
    if (_current_size > 0) then
      new_packet()
    end
    _buffer.push(bytes)
    new_packet()

  fun ref u8(value: U8): WriteBuffer =>
    _byte(value)

  fun ref i8(value: I8): WriteBuffer =>
    _byte(value.u8())

  fun ref u16_be(value: U16): WriteBuffer =>
    _byte((value>>8).u8())._byte(value.u8())

  fun ref i16_be(value: I16): WriteBuffer =>
    u16_be(value.u16())

  fun ref u32_be(value: U32): WriteBuffer =>
    u16_be((value>>16).u16()).u16_be(value.u16())

  fun ref i32_be(value: I32): WriteBuffer =>
    u32_be(value.u32())

  fun ref u64_be(value: U64): WriteBuffer =>
    u32_be((value>>32).u32()).u32_be(value.u32())

  fun ref i64_be(value: I64): WriteBuffer =>
    u64_be(value.u64())

  fun ref u16_le(value: U16): WriteBuffer =>
    _byte(value.u8())._byte((value>>8).u8())

  fun ref i16_le(value: I16): WriteBuffer =>
    u16_le(value.u16())

  fun ref u32_le(value: U32): WriteBuffer =>
    u16_le(value.u16()).u16_le((value>>16).u16())

  fun ref i32_le(value: I32): WriteBuffer =>
    u32_le(value.u32())

  fun ref u64_le(value: U64): WriteBuffer =>
    u32_le(value.u32()).u32_le((value>>32).u32())

  fun ref i64_le(value: I64): WriteBuffer =>
    u64_le(value.u64())
