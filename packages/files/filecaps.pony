use "collections"

primitive FileCreate
  fun value(): U32 => 1 << 0

primitive FileChmod
  fun value(): U32 => 1 << 1

primitive FileChown
  fun value(): U32 => 1 << 2

primitive FileLink
  fun value(): U32 => 1 << 3

primitive FileLookup
  fun value(): U32 => 1 << 4

primitive FileMkdir
  fun value(): U32 => 1 << 5

primitive FileRead
  fun value(): U32 => 1 << 6

primitive FileRemove
  fun value(): U32 => 1 << 7

primitive FileRename
  fun value(): U32 => 1 << 8

primitive FileSeek
  fun value(): U32 => 1 << 9

primitive FileStat
  fun value(): U32 => 1 << 10

primitive FileSync
  fun value(): U32 => 1 << 11

primitive FileTime
  fun value(): U32 => 1 << 12

primitive FileTruncate
  fun value(): U32 => 1 << 13

primitive FileWrite
  fun value(): U32 => 1 << 14

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
  ), U32]
