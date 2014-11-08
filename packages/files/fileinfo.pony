class FileInfo val
  """
  This contains file system metadata for a path. The times are in the same
  format as Time.now(), i.e. seconds and nanoseconds since the epoch.

  The UID and GID are UNIX-style user and group IDs. These will be zero on
  Windows. The change_time will actually be the file creation time on Windows.
  """
  let path: String

  let mode: FileMode val = recover FileMode end
  let hard_links: U32 = 0
  let uid: U32 = 0
  let gid: U32 = 0
  let size: U64 = 0
  let access_time: (I64, I64) = (0, 0)
  let modified_time: (I64, I64) = (0, 0)
  let change_time: (I64, I64) = (0, 0)

  let file: Bool = false
  let directory: Bool = false
  let pipe: Bool = false
  let symlink: Bool = false

  new create(from: String) ? =>
    path = from

    if not @os_stat[Bool](from.cstring(), this) then
      error
    end
