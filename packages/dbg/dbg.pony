use @dbg_ctx_create[DbgCtx](number_of_bits: USize)
use @dbg_ctx_create_with_dst_file[DbgCtx](number_of_bits: USize,
      dst: Pointer[U8])
use @dbg_ctx_create_with_dst_buf[DbgCtx](number_of_bits: USize,
      dst_size: USize, tmp_buf_size: USize)
use @dbg_ctx_destroy[None](dc: DbgCtx box)
use @dbg_set_bit[None](dc: DbgCtx, bits_idx: USize, value: Bool)
use @dbg_get_bit[Bool](dc: DbgCtx, bits_idx: USize)
use @dbg_printf[None](dc: DbgCtx, str: Pointer[U8] tag)
use @dbg_read[USize](dc: DbgCtx, dst: Pointer[U8] tag, dst_size: USize,
      size: USize)

use @malloc[Pointer[U8]](len: USize)
use @free[None](ptr: Pointer[U8] box)

struct DbgCtx
  var bits: Pointer[USize] = Pointer[USize]

  var dst_file: Pointer[U8] = Pointer[U8]

  var dst_buf: Pointer[U8] = Pointer[U8]
  var dst_buf_size: USize = 0
  var dst_buf_begi: USize = 0
  var dst_buf_endi: USize = 0
  var dst_buf_cnt: USize = 0

  var tmp_buf: Pointer[U8] = Pointer[U8]
  var tmp_buf_size: USize = 0
  var max_size: USize = 0

class DbgReadBuf
  let _buf: Pointer[U8]
  let _size: USize

  new create(sz: USize) =>
    _size = sz
    _buf = @malloc(_size)

  fun apply(): this->Pointer[U8] =>
    _buf

  fun size(): USize =>
    _size

  fun ref string(): String ref =>
    String.from_cstring(_buf)

  fun _final() =>
    @free(_buf)

class DC
  let _dc: DbgCtx
  let _dst_stream: (OutStream | None)

  new create(number_of_bits: USize) =>
    _dst_stream = None
    _dc = @dbg_ctx_create(number_of_bits)

  new create_with_dst_stdout(number_of_bits: USize) =>
    _dst_stream = None
    _dc = @dbg_ctx_create_with_dst_file(number_of_bits,
        @pony_os_stdout[Pointer[U8]]())

  new create_with_dst_stderr(number_of_bits: USize) =>
    _dst_stream = None
    _dc = @dbg_ctx_create_with_dst_file(number_of_bits,
        @pony_os_stderr[Pointer[U8]]())

  new create_with_dst_buf(number_of_bits: USize, dst_size: USize,
    tmp_buf_size: USize = 0x100)
  =>
    _dst_stream = None
    _dc = @dbg_ctx_create_with_dst_buf(number_of_bits, dst_size, tmp_buf_size)

  new create_with_OutStream(number_of_bits: USize, stream: OutStream) =>
    _dst_stream = stream
    _dc = @dbg_ctx_create(number_of_bits)

  fun ref apply(bit_idx: USize): Bool =>
    gb(bit_idx)

  fun ref sb(bit_idx: USize, bit_value: Bool) =>
    @dbg_set_bit(_dc, bit_idx, bit_value)

  fun ref gb(bit_idx: USize): Bool =>
    let r: Bool = @dbg_get_bit(_dc, bit_idx)
    r

  fun ref print(str: String): Bool =>
    if _dst_stream is None then
      @dbg_printf(_dc, str.cstring())
    else
      try
        (_dst_stream as OutStream).print(str)
      end
    end
    true

  fun ref read(buf: DbgReadBuf, len: USize = USize.max_value()): USize =>
    @dbg_read(_dc, buf(), buf.size(),
        if len < buf.size() then len else buf.size() - 1 end)

  fun _final() =>
    @dbg_ctx_destroy(_dc)
