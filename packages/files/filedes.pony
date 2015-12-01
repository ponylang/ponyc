use "time"
use "capsicum"

primitive _FileDes
  """
  Convenience operations on file descriptors.
  """
  fun chmod(fd: I32, path: FilePath, mode: FileMode box): Bool =>
    """
    Set the FileMode for this fd.
    """
    if not path.caps(FileChmod) or (fd == -1) then
      return false
    end

    ifdef windows then
      path.chmod(mode)
    else
      @fchmod[I32](fd, mode._os()) == 0
    end

  fun chown(fd: I32, path: FilePath, uid: U32, gid: U32): Bool =>
    """
    Set the owner and group for this file. Does nothing on Windows.
    """
    ifdef windows then
      false
    else
      if (fd != -1) and path.caps(FileChown) then
        @fchown[I32](fd, uid, gid) == 0
      else
        false
      end
    end

  fun touch(fd: I32, path: FilePath): Bool =>
    """
    Set the last access and modification times of the file to now.
    """
    set_time(fd, path, Time.now(), Time.now())

  fun set_time(fd: I32, path: FilePath, atime: (I64, I64),
    mtime: (I64, I64)): Bool
  =>
    """
    Set the last access and modification times of the file to the given values.
    """
    if (fd == -1) or not path.caps(FileTime) then
      return false
    end

    ifdef windows then
      path.set_time(atime, mtime)
    else
      var tv: (ILong, ILong, ILong, ILong) =
        (atime._1.ilong(), atime._2.ilong() / 1000,
          mtime._1.ilong(), mtime._2.ilong() / 1000)
      @futimes[I32](fd, addressof tv) == 0
    end

  fun set_rights(fd: I32, path: FilePath, writeable: Bool = true) ? =>
    """
    Set the Capsicum rights on the file descriptor.
    """
    ifdef freebsd then
      if fd != -1 then
        let cap = CapRights.from(path.caps)

        if not writeable then
          cap.unset(Cap.write())
        end

        if not cap.limit(fd) then
          error
        end
      end
    end
