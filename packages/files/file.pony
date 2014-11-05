primitive _FileHandle

class File
  var _path: String
  var _handle: Pointer[_FileHandle]
  var _write: Bool
  var _last_line_length: U64 = 256

  // Open for read/write, creating if it doesn't exist, truncating it if it
  // does exist.
  new create(path: String) ? =>
    _path = path
    _handle = @fopen[Pointer[_FileHandle]](path.cstring(), "w+b".cstring())
    _write = true

    if _handle.u64() == 0 then
      error
    end

  // Open for read only, failing if it doesn't exist.
  new open(path: String) ? =>
    _path = path
    _handle = @fopen[Pointer[_FileHandle]](path.cstring(), "rb".cstring())
    _write = false

    if _handle.u64() == 0 then
      error
    end

  // Open for read/write, creating if it doesn't exist, preserving the contents
  // if it does exist.
  new modify(path: String) ? =>
    _path = path
    _handle = @fopen[Pointer[_FileHandle]](path.cstring(), "r+b".cstring())
    _write = true

    if _handle.u64() == 0 then
      error
    end

  // Returns true if the file is currently open.
  fun box valid(): Bool => _handle.u64() != 0

  // Close the file. Future operations will do nothing. If this isn't done,
  // the underlying file descriptor will leak.
  fun ref close() =>
    if _handle.u64() != 0 then
      @fclose[I32](_handle)
      _handle = Pointer[_FileHandle].null()
    end

  // Flush the file.
  fun ref flush(): File =>
    if _handle.u64() != 0 then
      if Platform.linux() then
        @fflush_unlocked[I32](_handle)
      else
        @fflush[I32](_handle)
      end
    end
    this

  // Returns a line as a String.
  fun ref line(): String =>
    if _handle.u64() != 0 then
      var offset = U64(0)
      var len = _last_line_length
      var result = recover String end

      repeat
        result.reserve(len)

        var r = if Platform.linux() then
          @fgets_unlocked[Pointer[U8]](
            result.cstring().u64() + offset, len - offset, _handle
            )
        else
          @fgets[Pointer[U8]](
            result.cstring().u64() + offset, len - offset, _handle
            )
        end

        result.recalc()

        var done = try
          (r.u64() == 0) or (result(result.length().i64() - 1) == 10)
        else
          true
        end

        if not done then
          offset = result.length()
          len = len * 2
        end
      until done end

      _last_line_length = len
      consume result
    else
      recover String end
    end

  // Returns up to len bytes.
  fun ref read(len: U64): Array[U8] iso^ =>
    if _handle.u64() != 0 then
      var result = recover Array[U8].undefined(len) end

      var r = if Platform.linux() then
        @fread_unlocked[U64](result.carray(), U64(1), len, _handle)
      else
        @fread[U64](result.carray(), U64(1), len, _handle)
      end

      result.truncate(r)
      consume result
    else
      recover Array[U8] end
    end

  // Returns false if the file wasn't opened with write permission.
  // Returns false and closes the file if not all the bytes were written.
  fun ref write(data: Array[U8] box): Bool =>
    if _write then
      if _handle.u64() != 0 then
        var len = if Platform.linux() then
          @fwrite_unlocked[U64](data.carray(), U64(1), data.length(), _handle)
        else
          @fwrite[U64](data.carray(), U64(1), data.length(), _handle)
        end

        if len == data.length() then
          return true
        end

        close()
      end
    end
    false

  // Return the current cursor position in the file.
  fun box position(): U64 =>
    if _handle.u64() != 0 then
      if Platform.windows() then
        @_ftelli64[U64](_handle)
      else
        @ftell[U64](_handle)
      end
    else
      U64(0)
    end

  // Set the cursor position relative to the start of the file.
  fun ref from_start(offset: U64): File =>
    if _handle.u64() != 0 then
      if Platform.windows() then
        @_fseeki64[I32](_handle, offset, I32(0))
      else
        @fseek[I32](_handle, offset, I32(0))
      end
    end
    this

  // Set the cursor position relative to the end of the file.
  fun ref from_end(offset: U64): File =>
    if _handle.u64() != 0 then
      if Platform.windows() then
        @_fseeki64[I32](_handle, -offset, I32(2))
      else
        @fseek[I32](_handle, -offset, I32(2))
      end
    end
    this

  // Move the cursor position.
  fun ref seek(offset: I64): File =>
    if _handle.u64() != 0 then
      if Platform.windows() then
        @_fseeki64[I32](_handle, offset, I32(1))
      else
        @fseek[I32](_handle, offset, I32(1))
      end
    end
    this

  fun ref lines(): FileLines => FileLines(this)

class FileLines is Iterator[String]
  var file: File
  var line: String

  new create(from: File) =>
    file = from
    line = file.line()

  fun ref has_next(): Bool => line.length() > 0

  fun ref next(): String => line = file.line()
