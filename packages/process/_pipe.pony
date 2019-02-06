use @pony_asio_event_create[AsioEventID](owner: AsioEventNotify, fd: U32,
      flags: U32, nsec: U64, noisy: Bool)
use @pony_asio_event_unsubscribe[None](event: AsioEventID)
use @pony_asio_event_destroy[None](event: AsioEventID)

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

primitive _ERRORBROKENPIPE
  fun apply(): I32 =>
    ifdef windows then 109
    else compile_error "no ERROR_BROKEN_PIPE" end

primitive _ERRORNODATA
  fun apply(): I32 =>
    ifdef windows then 232
    else compile_error "no ERROR_NO_DATA" end

class _Pipe
  var near_fd: U32
  var far_fd: U32
  var event: AsioEventID = AsioEvent.none()

  new none() =>
    near_fd = -1
    far_fd = -1

  new create(outgoing: Bool) ? =>
    """
    Creates a pipe, an unidirectional data channel that can be used for
    interprocess communication. Outgoing pipes are written to by this process,
    incoming pipes are read from by this process.
    """
    ifdef posix then
      var fds = (U32(0), U32(0))
      if @pipe[I32](addressof fds) < 0 then
        error
      end
      if outgoing then
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
    else
      compile_error "unsupported platform"
    end

  fun _set_fd(fd: U32, flags: I32) ? =>
    let result = @fcntl[I32](fd, _FSETFD(), flags)
    if result < 0 then error end

  fun _set_fl(fd: U32, flags: I32) ? =>
    let result = @fcntl[I32](fd, _FSETFL(), flags)
    if result < 0 then error end

  fun ref begin(owner: AsioEventNotify, outgoing: Bool) =>
    """
    Prepare the pipe for read or write, and listening, after the far end has
    been handed to the other process.
    """
    ifdef posix then
      let flags = if outgoing then AsioEvent.write() else AsioEvent.read() end
      event = @pony_asio_event_create(owner, near_fd, flags, 0, true)
      close_far()
    end

  fun ref close_far() =>
    """
    Close the far end of the pipe--the end that the other process will be
    using. This is used to cleanup this process' handles that it wont use.
    """
    if far_fd != -1 then
      @close[I32](far_fd)
      far_fd = -1
    end

  fun ref read(read_buf: Array[U8] iso, read_len: USize): (Array[U8] iso^, ISize, I32) =>
    ifdef posix then
      let len =
        @read[ISize](near_fd, read_buf.cpointer().usize() + read_len,
          read_buf.size() - read_len)
      if len == -1 then // OS signals write error
        (consume read_buf, len, @pony_os_errno())
      else
        (consume read_buf, len, 0)
      end
    else
      compile_error "unsupported platform"
    end

  fun ref write(data: ByteSeq box, offset: USize): (ISize, I32) =>
    let len = @write[ISize](near_fd, data.cpointer().usize() + offset, data.size() - offset)
    if len == -1 then // OS signals write error
      (len, @pony_os_errno())
    else
      (len, 0)
    end

  fun ref is_closed(): Bool =>
    near_fd == -1

  fun ref close_near() =>
    """
    Close the near end of the pipe--the end that this process is using
    directly. Also handle unsubscribing the asio event (if there was one). File
    descriptors should always be closed _after_ unsubscribing its
    event, otherwise there is the possibility of reusing the file
    descriptor in another thread and then unsubscribing the reused
    file descriptor here!  Unsubscribing and closing the file
    descriptor should be treated as one operation.
    """
    if near_fd != -1 then
      if event isnt AsioEvent.none() then
        @pony_asio_event_unsubscribe(event)
      end
      @close[I32](near_fd)
      near_fd = -1
    end

  fun ref close() =>
    close_far()
    close_near()

  fun ref dispose() =>
    @pony_asio_event_destroy(event)
    event = AsioEvent.none()
