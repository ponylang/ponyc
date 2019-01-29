use "debug"

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
    Debug("create:: outgoing: " + outgoing.string())
    ifdef posix then
      var fds = (U32(0), U32(0))
      if @pipe[I32](addressof fds) < 0 then
        Debug("  create pipe failed")
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

    elseif windows then
      near_fd = 0
      far_fd = 0
      // create the pipe and set one handle to not inherit. That needs
      // to be done with the knowledge of which way this pipe goes.
      if @ponyint_win_pipe_create[U32](addressof near_fd, addressof far_fd, outgoing) == 0 then
        Debug("  create pipe failed")
        error
      end
    else
      // Unknown platform
      compile_error "unsupported platform"
    end
    Debug("  created pipe, near_fd: " + near_fd.string() + ", far_fd: " + far_fd.string())

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
      Debug("begin:: near_fd: " + near_fd.string() + ", outgoing: " + outgoing.string())
      let flags = if outgoing then AsioEvent.write() else AsioEvent.read() end
      event = @pony_asio_event_create(owner, near_fd, flags, 0, true)
      Debug("  created event: " + event.usize().string())
      close_far()
    end

  fun ref close_far() =>
    """
    Close the far end of the pipe--the end that the other process will be
    using. This is used to cleanup this process' handles that it wont use.
    """
    if far_fd != -1 then
      Debug("closed_far:: far_fd: " + far_fd.string())
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
    else // windows
      let hnd: USize = @_get_osfhandle[USize](near_fd)
      var bytes_read: U32 = 0
      let ok =
        @ReadFile[Bool](hnd, read_buf.cpointer().usize() + read_len,
          (read_buf.size() - read_len).u32(), addressof bytes_read, USize(0))
      let winerr = @GetLastError[I32]()
      Debug("read:: nead_fd: " + near_fd.string() + ", bytes_read: " + bytes_read.string() + ", ok:" + ok.string() + ", winerr:" + winerr.string())
      if not ok then
        if winerr == _ERRORBROKENPIPE() then
          (consume read_buf, 0, 0) // The pipe has been ended.
        elseif winerr == _ERRORNODATA() then
          (consume read_buf, 0, 0) // The pipe is being closed.
        else
          (consume read_buf, -1, _EINTR()) // Some other error, just make up one
        end
        //ERROR_MORE_DATA(234)? ERROR_PIPE_NOT_CONNECTED(233)?
      else
        (consume read_buf, bytes_read.isize(), 0) // read some
      end
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
      Debug("close_near:: near_fd: " + near_fd.string())
      if event isnt AsioEvent.none() then
        Debug("  unsubscribing event: " + event.usize().string())
        @pony_asio_event_unsubscribe(event)
      end
      @close[I32](near_fd)
      Debug("  closed pipe near_fd: " + near_fd.string())
      near_fd = -1
    end

  fun ref close() =>
    Debug("close:: near_fd: " + near_fd.string() + ", far_fd: " + far_fd.string())
    close_far()
    close_near()

  fun ref dispose() =>
    Debug("dispose:: near_fd: " + near_fd.string() + ", event: " + event.usize().string())
    @pony_asio_event_destroy(event)
    Debug("  disposed event & noning: " + event.usize().string())
    event = AsioEvent.none()
