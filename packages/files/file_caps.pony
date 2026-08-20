use "collections"

primitive FileCreate
  """
  Grants permission to create files and directories.
  """
  fun value(): U32 => 1 << 0

primitive FileChmod
  """
  Grants permission to change file permissions.
  """
  fun value(): U32 => 1 << 1

primitive FileChown
  """
  Grants permission to change file ownership.
  """
  fun value(): U32 => 1 << 2

primitive FileLink
  """
  Grants permission to create hard and symbolic links.
  """
  fun value(): U32 => 1 << 3

primitive FileLookup
  """
  Grants permission to look up files in a directory.
  """
  fun value(): U32 => 1 << 4

primitive FileMkdir
  """
  Grants permission to create directories.
  """
  fun value(): U32 => 1 << 5

primitive FileRead
  """
  Grants permission to read file contents.
  """
  fun value(): U32 => 1 << 6

primitive FileRemove
  """
  Grants permission to remove files and directories.
  """
  fun value(): U32 => 1 << 7

primitive FileRename
  """
  Grants permission to rename files.
  """
  fun value(): U32 => 1 << 8

primitive FileSeek
  """
  Grants permission to seek within a file.
  """
  fun value(): U32 => 1 << 9

primitive FileStat
  """
  Grants permission to query file metadata.
  """
  fun value(): U32 => 1 << 10

primitive FileSync
  """
  Grants permission to sync file contents to disk.
  """
  fun value(): U32 => 1 << 11

primitive FileTime
  """
  Grants permission to set file modification times.
  """
  fun value(): U32 => 1 << 12

primitive FileTruncate
  """
  Grants permission to truncate file contents.
  """
  fun value(): U32 => 1 << 13

primitive FileWrite
  """
  Grants permission to write to files.
  """
  fun value(): U32 => 1 << 14

primitive FileExec
  """
  Grants permission to execute a file.
  """
  fun value(): U32 => 1 << 15

type FileCaps is Flags[
  ( FileCreate
  | FileChmod
  | FileChown
  | FileLink
  | FileLookup
  | FileMkdir
  | FileRead
  | FileRemove
  | FileRename
  | FileSeek
  | FileStat
  | FileSync
  | FileTime
  | FileTruncate
  | FileWrite
  | FileExec
  ),
  U32 ]
