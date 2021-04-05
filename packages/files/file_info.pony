use @pony_os_stat[Bool](path: Pointer[U8] tag, file: FileInfo tag)
use @pony_os_fstat[Bool](fd: I32, path: Pointer[U8] tag, file: FileInfo tag)
use @pony_os_fstatat[Bool](fd: I32, path: Pointer[U8] tag, file: FileInfo tag)

class val FileInfo
  """
  This contains file system metadata for a path.

  A symlink will report information about itself, other than the size which
  will be the size of the target. A broken symlink will report as much as it
  can and will set the broken flag.
  """
  let filepath: FilePath

  let mode: FileMode val = recover FileMode end
    """UNIX-style file mode."""

  let hard_links: U32 = 0
    """Number of hardlinks to this `filepath`."""

  let device: U64 = 0
    """
    OS id of the device containing this `filepath`.
    Device IDs consist of a major and minor device id,
    denoting the type of device and the instance of this type on the system.
    """

  let inode: U64 = 0
    """UNIX specific INODE number of `filepath`. Is 0 on Windows."""

  let uid: U32 = 0
    """UNIX-style user ID of the owner of `filepath`."""

  let gid: U32 = 0
    """UNIX-style user ID of the owning group of `filepath`."""

  let size: USize = 0
    """
    Total size of `filepath` in bytes.

    In case of a symlink this is the size of the target, not the symlink itself.
    """

  let access_time: (I64, I64) = (0, 0)
    """
    Time of last access as a tuple of seconds and nanoseconds since the epoch:

    ```pony
    (let a_secs: I64, let a_nanos: I64) = file_info.access_time
    ```
    """

  let modified_time: (I64, I64) = (0, 0)
    """
    Time of last modification as tuple of seconds and nanoseconds since the epoch:

    ```pony
    (let m_secs: I64, let m_nanos: I64) = file_info.modified_time
    ```
    """

  let change_time: (I64, I64) = (0, 0)
    """
    Time of the last change either the attributes (number of links, owner,
    group, file mode, ...) or the content of `filepath`
    as a tuple of seconds and nanoseconds since the epoch:

    ```pony
    (let c_secs: I64, let c_nanos: I64) = file_info.change_time
    ```

    On Windows this will be the file creation time.
    """

  let file: Bool = false
    """`true` if `filepath` points to an a regular file."""

  let directory: Bool = false
    """`true` if `filepath` points to a directory."""

  let pipe: Bool = false
    """`true` if `filepath` points to a named pipe."""

  let symlink: Bool = false
    """`true` if `filepath` points to a symbolic link."""

  let broken: Bool = false
    """`true` if `filepath` points to a broken symlink."""

  new val create(from: FilePath) ? =>
    """
    This will raise an error if the FileStat capability isn't available or the
    path doesn't exist.
    """
    if not from.caps(FileStat) then
      error
    end

    filepath = from

    if not @pony_os_stat(from.path.cstring(), this) then
      error
    end

  new val _descriptor(fd: I32, path: FilePath) ? =>
    """
    This will raise an error if the FileStat capability isn't available or the
    file descriptor is invalid.
    """
    if not path.caps(FileStat) or (fd == -1) then
      error
    end

    filepath = path

    let fstat =
      @pony_os_fstat(fd, path.path.cstring(), this)
    if not fstat then error end

  new val _relative(fd: I32, path: FilePath, from: String) ? =>
    if not path.caps(FileStat) or (fd == -1) then
      error
    end

    filepath = path

    let fstatat =
      @pony_os_fstatat(fd, from.cstring(), this)
    if not fstatat then error end
