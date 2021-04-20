use "buffered"

class FileLines[D: StringDecoder = UTF8StringDecoder] is Iterator[String iso^]
  """
  Iterate over the lines in a file.

  Returns lines without trailing line breaks.
  Advances the file cursor to the end of each line returned from `next`.

  This class buffers the file contents to accumulate full lines. If the file
  does not contain linebreaks, the whole file content is read and buffered, which
  might exceed memory resources. Take care.
  """
  let _reader: Reader = Reader
  let _file: File
  let _min_read_size: USize
  var _last_line_length: USize
  var _buffer_cursor: USize
    """Internal cursor for keeping track until where in the file we already buffered."""
  var _cursor: USize
    """Keeps track of the file position we update after every returned line."""
  var _has_next: Bool

  new create(file: File, min_read_size: USize = 256) =>
    """
    Create a FileLines instance on a given file.

    This instance returns lines from the position of the given `file`
    at the time this constructor is called. Later manipulation of the file position
    is not accounted for. As a result iterating with this class will always return the full
    file content without gaps or repeated lines.

    `min_read_size` determines the minimum amount of bytes to read from the file
    in one go. This class keeps track of the line lengths in the current file
    and uses the length of the last line as amount of bytes to read next, but it
    will never read less than `min_read_size`.
    """
    _file = file
    _buffer_cursor = _file.position()
    _cursor = _file.position()
    _min_read_size = min_read_size
    _last_line_length = min_read_size
    _has_next = _file.valid()

  fun ref has_next(): Bool =>
    _has_next

  fun ref next(): String iso^ ? =>
    """
    Returns the next line in the file.
    """
    while true do
      try
        return _read_line()?
      else
        if not _fill_buffer() then
          // nothing to read from file, we can savely exit here
          break
        end
      end
    end
    _has_next = false
    if _reader.size() > 0 then
      // don't forget the last line
      _read_last_line()?
    else
      // nothing to return, we can only error here
      error
    end

  fun ref _read_line(): String iso^ ? =>
    (let line, let len) = _reader.line[D](where keep_line_breaks = true)?
    _last_line_length = len

    // advance the cursor to the end of the returned line
    _inc_public_file_cursor(len)

    // strip trailing line break
    line.truncate(
      len - if (len >= 2) and (line.at_offset(-2)? == '\r') then 2 else 1 end)
    consume line

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
      let read_bytes = _last_line_length.max(_min_read_size)
      let read_buf = _file.read(read_bytes)
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

  fun ref _read_last_line(): String iso^ ? =>
    let block = _reader.block(_reader.size())?
    _inc_public_file_cursor(block.size())
    String.from_iso_array[D](consume block)

  fun ref _inc_public_file_cursor(amount: USize) =>
    _cursor = _cursor + amount
    _file.seek_start(_cursor)
