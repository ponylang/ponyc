class FileMode
  var setuid: Bool = false
  var setgid: Bool = false
  var sticky: Bool = false
  var owner_read: Bool = false
  var owner_write: Bool = false
  var owner_exec: Bool = false
  var group_read: Bool = false
  var group_write: Bool = false
  var group_exec: Bool = false
  var any_read: Bool = false
  var any_write: Bool = false
  var any_exec: Bool = false

class FileInfo val
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
