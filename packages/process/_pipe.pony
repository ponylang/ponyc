use @fcntl[I32](fd: U32, cmd: I32, ...)
use @pipe[I32](fds: Pointer[(U32, U32)]) if posix
use @ponyint_win_pipe_create[U32](near_fd: Pointer[U32] tag,
  far_fd: Pointer[U32] tag, outgoing: Bool) if windows
use @close[I32](fd: U32)
use @read[ISize](fd: U32, buf: Pointer[U8] tag, size: USize) if posix
use @_get_osfhandle[Pointer[None]](fd: U32) if not posix
use @PeekNamedPipe[Bool](pipeHandle: Pointer[None], buffer: Pointer[None],
  bufferSize: U32, bytesRead: Pointer[None], bytesAvail: Pointer[None],
  bytesLeft: Pointer[None]) if not posix
use @ReadFile[Bool](handle: Pointer[None], buffer: Pointer[None],
  bytes_to_read: U32, bytes_read: Pointer[U32], overlapped: Pointer[None])
  if not posix
use @WriteFile[Bool](handle: Pointer[None], buffer: Pointer[None],
  bytes_to_write: U32, bytes_written: Pointer[U32], overlapped: Pointer[None])
  if not posix
use @GetLastError[I32]() if not posix

primitive _FSETFL
  fun apply(): I32 => 4

primitive _FGETFL
  fun apply(): I32 => 3

primitive _FSETFD
  fun apply(): I32 => 2

primitive _FGETFD
  fun apply(): I32 => 1

primitive _FDCLOEXEC
  fun apply(): I32 => 1

// Non-blocking IO file status flag
primitive _ONONBLOCK
  fun apply(): I32 =>
    ifdef bsd or osx then 4
    elseif linux then 2048
    else compile_error "no O_NONBLOCK" end

// The pipe has been ended.
primitive _ERRORBROKENPIPE
  fun apply(): I32 =>
    ifdef windows then 109
    else compile_error "no ERROR_BROKEN_PIPE" end

// The pipe is being closed.
primitive _ERRORNODATA
  fun apply(): I32 =>
    ifdef windows then 232
    else compile_error "no ERROR_NO_DATA" end

class _Pipe
  """
  A pipe is a unidirectional data channel that can be used for interprocess
  communication. Outgoing pipes are written to by this process, incoming pipes
  are read from by this process.
  """
  let _outgoing: Bool
  var near_fd: U32 = -1
  var far_fd: U32 = -1
  var event: AsioEventID = AsioEvent.none()

  new none() =>
    """
    Creates a nil pipe for use as a placeholder.
    """
    _outgoing = true

  new outgoing() ? =>
    """
    Creates an outgoing pipe.
    """
    _outgoing = true
    _create()?

  new incoming() ? =>
    """
    Creates an incoming pipe.
    """
    _outgoing = false
    _create()?

  fun ref _create() ? =>
    """
    Do the actual system object creation for the pipe.
    """
    ifdef posix then
      var fds = (U32(0), U32(0))
      if @pipe(addressof fds) < 0 then
        error
      end
      if _outgoing then
        near_fd = fds._2
        far_fd = fds._1
      else
        near_fd = fds._1
        far_fd = fds._2
      end
      // We need to set the flag on the two file descriptors here to prevent
      // capturing by another thread.
      _set_fd(near_fd, _FDCLOEXEC())?
      _set_fd(far_fd, _FDCLOEXEC())?
      _set_fl(near_fd, _ONONBLOCK())? // and non-blocking on parent side of pipe

    elseif windows then
      near_fd = 0
      far_fd = 0
      // create the pipe and set one handle to not inherit. That needs
      // to be done with the knowledge of which way this pipe goes.
      if @ponyint_win_pipe_create(addressof near_fd, addressof far_fd,
          _outgoing) == 0 then
        error
      end
    else
      compile_error "unsupported platform"
    end

  fun _set_fd(fd: U32, flags: I32) ? =>
    let result = @fcntl(fd, _FSETFD(), flags)
    if result < 0 then error end

  fun _set_fl(fd: U32, flags: I32) ? =>
    let result = @fcntl(fd, _FSETFL(), flags)
    if result < 0 then error end

  fun ref begin(owner: AsioEventNotify) =>
    """
    Prepare the pipe for read or write, and listening, after the far end has
    been handed to the other process.
    """
    ifdef posix then
      let flags = if _outgoing then AsioEvent.write() else AsioEvent.read() end
      event = AsioEvent.create_event(owner, near_fd, flags, 0, true)
    end
    close_far()

  fun ref close_far() =>
    """
    Close the far end of the pipe--the end that the other process will be
    using. This is used to cleanup this process' handles that it wont use.
    """
    if far_fd != -1 then
      @close(far_fd)
      far_fd = -1
    end

  fun ref read(read_buf: Array[U8] iso, offset: USize):
    (Array[U8] iso^, ISize, I32)
  =>
    ifdef posix then
      let len =
        @read(near_fd, read_buf.cpointer(offset),
          read_buf.size() - offset)
      if len == -1 then // OS signals write error
        (consume read_buf, len, @pony_os_errno())
      else
        (consume read_buf, len, 0)
      end
    else // windows
      let hnd = @_get_osfhandle(near_fd)
      var bytes_to_read: U32 = (read_buf.size() - offset).u32()
      // Peek ahead to see if there is anything to read, return if not
      var bytes_avail: U32 = 0
      let okp = @PeekNamedPipe(hnd, USize(0), bytes_to_read, USize(0),
          addressof bytes_avail, USize(0))
      let winerrp = @GetLastError()
      if not okp then
        if (winerrp == _ERRORBROKENPIPE()) then
          // Pipe is done & ready to close.
          return (consume read_buf, 0, 0)
        elseif (winerrp == _ERRORNODATA()) then
          // We are using a non-blocking read. That will return a _ERRORNODATA
          // if the pipe is currently empty. There could be data later so we
          // return _EAGAIN
          return (consume read_buf, -1, _EAGAIN())
        else
          // Some other error, map to invalid arg
          return (consume read_buf, -1, _EINVAL())
        end
      elseif bytes_avail == 0 then
        // Peeked ok, but nothing to read. Return and try again later.
        return (consume read_buf, -1, _EAGAIN())
      end
      if bytes_to_read > bytes_avail then
        bytes_to_read = bytes_avail
      end
      // Read up to the bytes available
      var bytes_read: U32 = 0
      let ok = @ReadFile(hnd, read_buf.cpointer(offset),
          bytes_to_read, addressof bytes_read, USize(0))
      let winerr = @GetLastError()
      if not ok then
        if (winerr == _ERRORBROKENPIPE()) then
          // Pipe is done & ready to close.
          (consume read_buf, 0, 0)
        else
          // Some other error, map to invalid arg
          (consume read_buf, -1, _EINVAL())
        end
      else
        // We know bytes_to_read is > 0, and can assume bytes_read is as well
        // buffer back, bytes read, no error
        (consume read_buf, bytes_read.isize(), 0)
      end
    end

  fun ref write(data: ByteSeq box, offset: USize): (ISize, I32) =>
    ifdef posix then
      let len = @write(near_fd, data.cpointer(offset),
        data.size() - offset)
      if len == -1 then // OS signals write error
        (len, @pony_os_errno())
      else
        (len, 0)
      end
    else // windows
      let hnd = @_get_osfhandle(near_fd)
      let bytes_to_write: U32 = (data.size() - offset).u32()
      var bytes_written: U32 = 0
      let ok = @WriteFile(hnd, data.cpointer(offset),
          bytes_to_write, addressof bytes_written, USize(0))
      let winerr = @GetLastError()
      if not ok then
        if (winerr == _ERRORBROKENPIPE()) then
          (0, 0) // Pipe is done & ready to close.
        elseif (winerr == _ERRORNODATA()) then
          // We are using a non-blocking write. That will return a _ERRORNODATA
          // if the pipe is currently unwriteable. It should be writeable later
          // so we return _EAGAIN
          (-1, _EAGAIN())
        else
          (-1, _EINVAL()) // Some other error, map to invalid arg
        end
      elseif bytes_written == 0 then
        (-1, _EAGAIN())
      else
        (bytes_written.isize(), 0)
      end
    end

  fun ref is_closed(): Bool =>
    near_fd == -1

  fun ref close_near() =>
    """
    Close the near end of the pipe--the end that this process is using
    directly. Also handle unsubscribing the asio event (if there was one). File
    descriptors should always be closed _after_ unsubscribing its event,
    otherwise there is the possibility of reusing the file descriptor in
    another thread and then unsubscribing the reused file descriptor here!
    Unsubscribing and closing the file descriptor should be treated as one
    operation.
    """
    if near_fd != -1 then
      if event isnt AsioEvent.none() then
        AsioEvent.unsubscribe(event)
      end
      @close(near_fd)
      near_fd = -1
    end

  fun ref close() =>
    close_far()
    close_near()

  fun ref dispose() =>
    AsioEvent.destroy(event)
    event = AsioEvent.none()
