use "collections"

class WriteBuffer
  """
  * A List[ByteSeq] maintained as WriteBuffer.
  * _current keeps track of the current packet `Array[U8] iso` into which the
  user is writing.
  * _current can potentially keep track of `ByteSeq iso` if push is conforming
  between Array[U8] and String.
    * _buffer is returned on take_buffer() as a ByteSeqIter
    * The current packet is pushed into _buffer on a call to new_packet()
    Example usage:
  ```pony
  use "random"
  use "net"

  class WBUDPNotify is UDPNotify
    let _data : ByteSeqIter
    let _env: Env
    let _ip: IPAddress
    new iso create(data: ByteSeqIter, ip: IPAddress, env:Env) =>
      _data = consume data
      _env = env
      _ip = ip

    fun ref listening(sock: UDPSocket ref) =>
      _env.out.print("Sending data")
      sock.writev(_data, _ip)
      sock.dispose()

  actor Main
    new create(env: Env) =>
      let wb: WriteBuffer iso = recover WriteBuffer end
      let mt = MT()
      wb.add_byte_seq("Hi there!\n")
      for x in "PRNs follow:\n".values() do
        wb.u8(x)
      end
      var i = U8(0)
      while (i < 10)do
        wb.u64_be(mt.next())
        i = i + 1
      end
      try
        let auth = env.root as AmbientAuth
        var ip: IPAddress =
          try
            DNS.ip4(auth, "127.0.0.1", "9999")(0)
          else
            return
          end
        let notify: WBUDPNotify iso = recover
          WBUDPNotify(wb.take_buffer(), ip, env)
        end
        UDPSocket(auth, consume notify)
      else return end
  ```
  """
  var _buffer: List[ByteSeq] iso
  var _current: Array[U8] iso
  var _current_size: USize

  new create() =>
    """
    Create _buffer, _current and set _current_size to 0
    """
    _buffer = recover List[ByteSeq] end
    _current = recover Array[U8] end
    _current_size = 0

  fun ref new_packet(): WriteBuffer =>
    """
    * Pushes _current into _buffer to ensure future writes go to a new packet
    * Accomplishes this with a destructive read of _current while pushing
      it onto _buffer.
    * Chainable
    * Empty packets disallowed for now
    """
    if (_current_size > 0) then
      _buffer.push(_current = recover Array[U8] end)
      _current_size = 0
    end
    this

  fun ref take_buffer(): ByteSeqIter =>
    """
    Destructive read of existing _buffer which is returned as ByteSeqIter
    """
    if (_current_size > 0) then new_packet() end
    _buffer = recover List[ByteSeq] end

  fun current_size(): USize =>
    """
    Size of _current
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
    """
    Add a ByteSeq. If current is 'dirty' get a new_packet()
    """
    if (_current_size > 0) then new_packet() end
    _buffer.push(bytes)
    this

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
