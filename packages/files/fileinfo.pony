class FileInfo val
  let path: String

  let mode: U32 = 0
  let hard_links: U32 = 0
  let uid: U32 = 0
  let gid: U32 = 0
  let size: U64 = 0
  let access_time: U64 = 0
  let modified_time: U64 = 0
  let change_time: U64 = 0

  let file: Bool = false
  let directory: Bool = false
  let pipe: Bool = false
  let symlink: Bool = false

  new create(from: String) ? =>
    path = from

    if not @os_stat[Bool](from.cstring(), this) then
      error
    end
