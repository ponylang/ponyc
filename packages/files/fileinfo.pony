class FileInfo val
  let path: String

  let mode: U32
  let hard_links: U32
  let uid: U32
  let gid: U32
  let size: U64
  let access_time: U64
  let modified_time: U64
  let change_time: U64

  let file: Bool
  let directory: Bool
  let pipe: Bool
  let symlink: Bool

  new create(from: String) ? =>
    path = from
    @os_stat[None](from.cstring(), this) ?
