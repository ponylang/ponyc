use "collections"

class WriteBuffer
  """
  A simple constant sized byte buffer to push Numbers into.
  The data container is iso in order to keep it sendable(?)
  Potential upgrades:
  1. Have a list of data buffers of preset size
  2. Arguably, seperate functions for _be and _le
  3. A better way to get data out of the buffer, without passing a new array?
  """
  var data: Array[U8] iso
  var _offset: USize
  var _little_endian: Bool

  new create(size: USize, little_endian: Bool = false) =>
    """
    Allocate the buffer
    """
    data = recover Array[U8].undefined(size) end
    _offset = 0
    _little_endian = little_endian

  fun ref get_data(x: Array[U8] iso): Array[U8] iso^ =>
    """
    Destructive read inorder to get an iso reference to data
    """
    var x': Array[U8] iso = data = consume x
    consume x'

  fun ref available(): USize =>
    """
    Number of bytes currently available
    """
    data.size() - _offset

  fun ref _byte(value: U8) : WriteBuffer^ ? =>
    """
    Write a single byte into the buffer if space available else error
    Update _offset
    """
    if _offset < data.size() then
      data(_offset) = value
      _offset = _offset + 1
    else error end
    this

  fun ref write_number(value: Number): WriteBuffer? =>
    """
    Write the number to buffer based on endianness
    """
    match (value, _little_endian)
    | (let v: U8, _) => _byte(v)
    | (let v: I8, _) => _byte(v.u8())
    | (let v: U16, false) => _byte((v>>8).u8()); _byte(v.u8())
    | (let v: I16, false) => _byte((v>>8).u8()); _byte(v.u8())
    | (let v: U32, false) => write_number((v>>16).u16()); write_number(v.u16())
    | (let v: I32, false) => write_number((v>>16).u16()); write_number(v.u16())
    | (let v: U64, false) => write_number((v>>32).u32()); write_number(v.u32())
    | (let v: I64, false) => write_number((v>>32).u32()); write_number(v.u32())
    | (let v: U16, true) => _byte(v.u8()); _byte((v>>8).u8())
    | (let v: I16, true) => _byte(v.u8()); _byte((v>>8).u8())
    | (let v: U32, true) => write_number(v.u16()); write_number((v>>16).u16())
    | (let v: I32, true) => write_number(v.u16()); write_number((v>>16).u16())
    | (let v: U64, true) => write_number(v.u32()); write_number((v>>32).u32())
    | (let v: I64, true) => write_number(v.u32()); write_number((v>>32).u32())
    else
      error
    end
