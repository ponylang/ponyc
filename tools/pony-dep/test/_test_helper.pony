use "buffered"
use "files"
use "pony_test"
use "time"

use @symlink[I32](target: Pointer[U8] tag, linkpath: Pointer[U8] tag)
use @chmod[I32](path: Pointer[U8] tag, mode: U32)

primitive \nodoc\ _TestHelper
  fun read_archive(path: FilePath): Reader ? =>
    let reader = Reader
    match OpenFile(path)
    | let f: File =>
      reader.append(f.read(f.size()))
      f.dispose()
      reader
    else
      error
    end

  fun tmp_dir(h: TestHelper): _TmpDirHolder ? =>
    let auth = FileAuth(h.env.root)
    let dir_path = FilePath(auth,
      "pony-dep-test-" + Time.nanos().string())
    if not dir_path.mkdir() then
      error
    end
    _TmpDirHolder(dir_path)

class \nodoc\ _TmpDirHolder
  let path: FilePath

  new create(path': FilePath) =>
    path = path'

  fun dispose() =>
    path.remove()
