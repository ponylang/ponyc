use "buffered"

class FileCharacters[D: StringDecoder = UTF8StringDecoder] is Iterator[U32]
  """
  Iterate over the characters in a file.
  """
  let _file: File
  let _reader: Reader = Reader
  let _buffer_size: USize
  var _buffer_cursor: USize
    """Internal cursor for keeping track until where in the file we already buffered."""
  var _cursor: USize
    """Keeps track of the file position we update after every returned line."""
  embed _decoder_bytes: StringDecoderBytes

new create(file: File, buffer_size: USize = 256) =>
  _file = file
  _buffer_size = buffer_size
  _buffer_cursor = _file.position()
  _cursor = _file.position()
  _decoder_bytes = StringDecoderBytes.create()

fun ref has_next(): Bool =>
  try
    _reader.peek_u8()?
  else
    if not _fill_buffer() then
      return false
    end
  end
  true

fun ref next(): U32 ? =>
  """
  Returns the next character in the file.
  """
  while true do
    try
      return _read()?
    else
      if not _fill_buffer() then
        // nothing to read from file, we can savely exit here
        break
      end
    end
  end
  error

fun ref _read(): U32 ? =>
  (let char, let sz) = _reader.codepoint[D]()?
  // advance the cursor to the end of the returned line
  _inc_public_file_cursor(sz.usize())
  char

fun ref _fill_buffer(): Bool =>
  """
  read from file and fill the reader-buffer.

  Returns `true` if data could be read from the file.

  After a successful reading operation `_buffer_cursor` is updated.
  """
  var result = true
  // get back to position of last line
  let current_pos = _file.position()
  _file.seek_start(_buffer_cursor)
  if _file.valid() then
    let read_buf = _file.read(_buffer_size)
    _buffer_cursor = _file.position()

    let errno = _file.errno()
    if (read_buf.size() == 0) and (errno isnt FileOK) then
      result = false
    else
      // TODO: Limit size of read buffer
      _reader.append(consume read_buf)
    end
  else
    result = false
  end
  // reset position to not disturb other operations on the file
  // we only actually advance the cursor if the line is returned.
  _file.seek_start(current_pos)
  result

fun ref _inc_public_file_cursor(amount: USize) =>
  _cursor = _cursor + amount
  _file.seek_start(_cursor)
