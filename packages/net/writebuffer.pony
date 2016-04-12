use "collections"

class WriteBuffer
  """
  * A List[ByteSeq] maintained as WriteBuffer.
  * _current keeps track of the current packet of type `ByteSeq iso`
    into which the user is writing
  * _buffer is returned on take_buffer() as a ByteSeqIter
  * The current packet is written into _buffer on call the new_packet()
  """
  var _buffer: List[ByteSeq] iso
  var _current: ByteSeq iso
  var _current_size: USize
  var _little_endian: Bool
  var _seq_type: ByteSeq

  new create(seq_type: ByteSeq, little_endian: Bool = false) =>
    """
    Create the buffer
    Create _current depending on the seq_type requested by the user
    """
    _buffer = recover List[ByteSeq] end
    _seq_type = seq_type
    _current =
      match seq_type
      | String => recover String end
      else recover Array[U8] end
      end
    _current_size = 0
    _little_endian = little_endian

  fun ref new_packet(): WriteBuffer =>
    """
    * Archives the current packet into _buffer and ensures future
      writes goto a new packet
    * Destructive read of _current and push it onto buffer.
    * Potentially, will have a default argument that will decide whether
      to have an Array[U8] or String as the next packet in the buffer
    * This is chainable
    """
    var x = _current = recover Array[U8 val] end
    _buffer.push(consume x)
    _current_size = 0
    this

  fun ref take_buffer(): ByteSeqIter =>
    """
    Destructive read of existing _buffer and return as ByteSeqIter
    """
    if (_current_size > 0) then
      new_packet()
    end
    var x = _buffer = recover List[ByteSeq] end
    consume x

  fun current_size(): USize =>
    """
    Size of current packet
    """
    _current_size

  fun ref _byte(value: U8 val) : WriteBuffer =>
    """
    Endless writes into _current
    Chainable
    Potentially use the push depending on the result of a match on _current
    """
    // match _seq_type
    // | String => _current.push(value)
    // | Array[U8] =>
    // end
    _current.push(value)
    _current_size = _current_size + 1
    this

  fun ref u8(value: U8) => _byte(value)

  fun ref i8(value: I8) => _byte(value.u8())

  fun ref u16(value: U16) => _byte((value>>8).u8()); _byte(value.u8())

  fun ref i16(value: I16) => _byte((value>>8).u8()); _byte(value.u8())

  fun ref u32(value: U32) => u16((value>>16).u16()); u16(value.u16())

  fun ref i32(value: I32) => u16((value>>16).u16()); u16(value.u16())

  fun ref u64(value: U64) => u32((value>>32).u32()); u32(value.u32())

  fun ref i64(value: I64) => u32((value>>32).u32()); u32(value.u32())
