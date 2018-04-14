use "ponytest"
use "collections"
use "buffered"

actor Main is TestList
  new create(env: Env) => PonyTest(env, this)
  new make() => None

  fun tag tests(test: PonyTest) =>
    test(_TestMkdtemp)
    test(_TestWalk)
    test(_TestDirectoryOpen)
    test(_TestPathClean)
    test(_TestPathJoin)
    test(_TestPathRel)
    test(_TestPathSplit)
    test(_TestPathDir)
    test(_TestPathBase)
    test(_TestPathExt)
    test(_TestPathVolume)
    test(_TestFileEOF)
    test(_TestFileOpenError)
    test(_TestFileCreate)
    test(_TestFileCreateExistsNotWriteable)
    test(_TestFileCreateDirNotWriteable)
    test(_TestFileCreateMissingCaps)
    test(_TestFileOpen)
    test(_TestFileOpenPermissionDenied)
    test(_TestFileOpenWrite)
    test(_TestFileLongLine)
    test(_TestFileWrite)
    test(_TestFileWritev)
    test(_TestFileQueue)
    test(_TestFileQueuev)
    test(_TestFileMixedWriteQueue)
    test(_TestFileWritevLarge)

primitive _FileHelper
  fun make_files(h: TestHelper, files: Array[String]): FilePath ? =>
    let top = Directory(FilePath.mkdtemp(h.env.root as AmbientAuth,
      "tmp._FileHelper.")?)?
    for f in files.values() do
      try
        // Since we embed paths, we use the posix separator, even on Windows.
        let dir_head = Path.split(f, "/")
        let fp = FilePath(top.path, dir_head._1)?
        let r = fp.mkdir()
        if dir_head._2 != "" then
          Directory(fp)?.create_file(dir_head._2)?.dispose()
        end
      else
        h.fail("Failed to create file: " + f)
        h.assert_true(top.path.remove())
        error
      end
    end
    top.path


class iso _TestMkdtemp is UnitTest
  fun name(): String => "files/FilePath.mkdtemp"
  fun apply(h: TestHelper) ? =>
    let tmp = FilePath.mkdtemp(h.env.root as AmbientAuth, "tmp.TestMkdtemp.")?
    try
      h.assert_true(FileInfo(tmp)?.directory)
    then
      h.assert_true(tmp.remove())
    end


class iso _TestWalk is UnitTest
  fun name(): String => "files/FilePath.walk"
  fun apply(h: TestHelper) ? =>
    let top =
      _FileHelper.make_files(h,
        ["a/1"; "a/2"; "b"; "c/3"; "c/4"; "d/5"; "d/6"])?
    try
      top.walk(
        {(dir: FilePath, entries: Array[String] ref)(p = top.path) =>
          if dir.path == p then
            h.assert_array_eq_unordered[String](["b"; "c"; "a"; "d"], entries)
          elseif dir.path.at("a", -1) then
            h.assert_array_eq_unordered[String](["1"; "2"], entries)
          elseif dir.path.at("c", -1) then
            h.assert_array_eq_unordered[String](["3"; "4"], entries)
          elseif dir.path.at("d", -1) then
            h.assert_array_eq_unordered[String](["5"; "6"], entries)
          else
            h.fail("Unexpected dir: " + dir.path)
          end
        })
    then
      h.assert_true(top.remove())
    end


class iso _TestDirectoryOpen is UnitTest
  fun name(): String => "files/File.open.directory"
  fun apply(h: TestHelper) ? =>
    let tmp = FilePath.mkdtemp(h.env.root as AmbientAuth, "tmp.TestDiropen.")?

    try
      h.assert_true(FileInfo(tmp)?.directory)
      with file = File.open(tmp) do
        h.assert_true(file.errno() is FileError)
        h.assert_false(file.valid())
      end
    then
      h.assert_true(tmp.remove())
    end


class iso _TestPathClean is UnitTest
  fun name(): String => "files/Path.clean"
  fun apply(h: TestHelper) =>
    let res1 = Path.clean("//foo/bar///")
    let res2 = Path.clean("foo/./bar")
    let res3 = Path.clean("foo/../foo")
    let res4 = Path.clean("///foo///bar///base.ext")

    ifdef windows then
      h.assert_eq[String](res1, "\\foo\\bar")
      h.assert_eq[String](res2, "foo\\bar")
      h.assert_eq[String](res3, "foo")
      h.assert_eq[String](res4, "\\foo\\bar\\base.ext")
    else
      h.assert_eq[String](res1, "/foo/bar")
      h.assert_eq[String](res2, "foo/bar")
      h.assert_eq[String](res3, "foo")
      h.assert_eq[String](res4, "/foo/bar/base.ext")
    end


class iso _TestPathJoin is UnitTest
  fun name(): String => "files/Path.join"
  fun apply(h: TestHelper) =>
    let path1 = "//foo/bar///"
    let path2 = "foo/./bar"
    let path3 = "foo/../foo"
    let path4 = "///foo///dir///base.ext"

    let res1 = Path.join(path1, path2)
    let res2 = Path.join(res1, path3)
    let res3 = Path.join(res2, path4)

    ifdef windows then
      h.assert_eq[String](res1, "\\foo\\bar\\foo\\bar")
      h.assert_eq[String](res2, "\\foo\\bar\\foo\\bar\\foo")
      h.assert_eq[String](res3, "\\foo\\dir\\base.ext")
    else
      h.assert_eq[String](res1, "/foo/bar/foo/bar")
      h.assert_eq[String](res2, "/foo/bar/foo/bar/foo")
      h.assert_eq[String](res3, "/foo/dir/base.ext")
    end


class iso _TestPathRel is UnitTest
  fun name(): String => "files/Path.rel"
  fun apply(h: TestHelper) ? =>
    let res = Path.rel("foo/bar", "foo/bar/baz")?
    h.assert_eq[String](res, "baz")


class iso _TestPathSplit is UnitTest
  fun name(): String => "files/Path.split"
  fun apply(h: TestHelper) =>
    ifdef windows then
      var path = "\\foo\\bar\\dir\\"
      let expect =
        [
          ("\\foo\\bar\\dir", "")
          ("\\foo\\bar", "dir")
          ("\\foo", "bar")
          (".", "foo")
          ("", ".")
        ]
      for parts in expect.values() do
        let res = Path.split(path)
        h.assert_eq[String](res._1, parts._1)
        h.assert_eq[String](res._2, parts._2)
        path = parts._1
      end

    else
      var path = "/foo/bar/dir/"
      let expect =
        [
          ("/foo/bar/dir", "")
          ("/foo/bar", "dir")
          ("/foo", "bar")
          (".", "foo")
          ("", ".")
        ]
      for parts in expect.values() do
        let res = Path.split(path)
        h.assert_eq[String](res._1, parts._1)
        h.assert_eq[String](res._2, parts._2)
        path = parts._1
      end
    end


class iso _TestPathDir is UnitTest
  fun name(): String => "files/Path.dir"
  fun apply(h: TestHelper) =>

    ifdef windows then
      let res1 = Path.dir("\\foo\\bar\\dir\\base.ext")
      let res2 = Path.dir("\\foo\\bar\\dir\\")
      h.assert_eq[String](res1, "\\foo\\bar\\dir")
      h.assert_eq[String](res2, "\\foo\\bar\\dir")

    else
      let res1 = Path.dir("/foo/bar/dir/base.ext")
      let res2 = Path.dir("/foo/bar/dir/")
      h.assert_eq[String](res1, "/foo/bar/dir")
      h.assert_eq[String](res2, "/foo/bar/dir")
    end


class iso _TestPathBase is UnitTest
  fun name(): String => "files/Path.dir"
  fun apply(h: TestHelper) =>

    (let p1, let p2) = ifdef windows then
      ("\\dir\\base.ext", "\\dir\\")
    else
      ("/dir/base.ext", "/dir/")
    end

    h.assert_eq[String](Path.base(p1), "base.ext")
    h.assert_eq[String](Path.base(p1, false), "base")
    h.assert_eq[String](Path.base(p2), "")


class iso _TestPathExt is UnitTest
  fun name(): String => "files/Path.ext"
  fun apply(h: TestHelper) =>
    ifdef windows then
      let res1 = Path.ext("\\dir\\base.ext")
      let res2 = Path.ext("\\dir\\base.ext\\")

      h.assert_eq[String](res1, "ext")
      h.assert_eq[String](res2, "")

    else
      let res1 = Path.ext("/dir/base.ext")
      let res2 = Path.ext("/dir/base.ext/")

      h.assert_eq[String](res1, "ext")
      h.assert_eq[String](res2, "")
    end


class iso _TestPathVolume is UnitTest
  fun name(): String => "files/Path.volume"
  fun apply(h: TestHelper) =>
    let res1 = Path.volume("C:\\foo")
    let res2 = Path.volume("\\foo")

    ifdef windows then
      h.assert_eq[String](res1, "C:")
      h.assert_eq[String](res2, "")
    else
      h.assert_eq[String](res1, "")
      h.assert_eq[String](res2, "")
    end


class iso _TestFileEOF is UnitTest
  fun name(): String => "files/File.eof-error"
  fun apply(h: TestHelper) =>
    try
      let path = "tmp.eof"
      let filepath = FilePath(h.env.root as AmbientAuth, path)?
      with file = File(filepath) do
        file.print("foobar")
        file.sync()
        file.seek_start(0)
        let line1 = file.line()?
        h.assert_eq[String]("foobar", consume line1)
        try
          let line2 = file.line()?
          h.fail("Read beyond EOF without error!")
        else
          h.assert_true(file.errno() is FileEOF)
        end
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end


class iso _TestFileCreate is UnitTest
  fun name(): String => "files/File.create"
  fun apply(h: TestHelper) =>
    try
      let path = "tmp.create"
      let filepath = FilePath(h.env.root as AmbientAuth, path)?
      with file = CreateFile(filepath) as File do
        file.print("foobar")
      end
      with file2 = CreateFile(filepath) as File do
        let line1 = file2.line()?
        h.assert_eq[String]("foobar", consume line1)
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end


class iso _TestFileCreateExistsNotWriteable is UnitTest
  fun name(): String => "files/File.create-exists-not-writeable"
  fun apply(h: TestHelper) =>
    try
      let content = "unwriteable"
      let path = "tmp.create-not-writeable"
      let filepath = FilePath(h.env.root as AmbientAuth, path)?
      let mode: FileMode ref = FileMode.>private()
      mode.owner_read = true
      mode.owner_write = false

      // preparing the non-writable, but readable file
      with file = CreateFile(filepath) as File do
        file.print(content)
        h.assert_true(file.chmod(mode))
      end

      with file2 = File(filepath) do
        h.assert_false(file2.valid())
        h.assert_is[FileErrNo](file2.errno(), FilePermissionDenied)

        let line = file2.line()?
        h.fail("read on invalid file succeeded")
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end


class iso _TestFileCreateDirNotWriteable is UnitTest
  fun name(): String => "files/File.create-dir-not-writeable"
  fun apply(h: TestHelper) =>
    try
      let dir_path =
        FilePath.mkdtemp(
          h.env.root as AmbientAuth,
          "tmp.create-dir-not-writeable")?
      let mode: FileMode ref = FileMode.>private()
      mode.owner_read = true
      mode.owner_write = false
      h.assert_true(dir_path.chmod(mode))

      try
        let file_path = dir_path.join("trycreateme")?
        let file = File(file_path)
        h.assert_false(file.valid())
        h.assert_true(file.writeable)
        h.assert_is[FileErrNo](file.errno(), FilePermissionDenied)
      then
        mode.owner_write = true
        h.assert_true(dir_path.chmod(mode))
        dir_path.remove()
      end
    else
      h.fail("Unhandled error!")
    end


class iso _TestFileCreateMissingCaps is UnitTest
  fun name(): String => "files/File.create-missing-caps"
  fun apply(h: TestHelper) =>
    try
      let no_create_caps = FileCaps.>all().>unset(FileCreate)
      let no_read_caps = FileCaps.>all().>unset(FileWrite)
      let no_write_caps = FileCaps.>all().>unset(FileRead)

      let file_path1 = FilePath(
        h.env.root as AmbientAuth,
        "tmp.create-missing-caps1",
        consume no_create_caps)?
      let file1 = File(file_path1)
      h.assert_false(file1.valid())
      h.assert_is[FileErrNo](file1.errno(), FileError)

      let file_path2 = FilePath(
        h.env.root as AmbientAuth,
        "tmp.create-missing-caps2",
        consume no_read_caps)?
      let file2 = File(file_path2)
      h.assert_false(file2.valid())
      h.assert_is[FileErrNo](file2.errno(), FileError)

      let file_path3 = FilePath(
        h.env.root as AmbientAuth,
        "tmp.create-missing-caps3",
        consume no_write_caps)?
      let file3 = File(file_path3)
      h.assert_false(file3.valid())
      h.assert_is[FileErrNo](file3.errno(), FileError)

    else
      h.fail("Unhandled error!")
    end

class iso _TestFileOpen is UnitTest
  fun name(): String => "files/File.open"
  fun apply(h: TestHelper) =>
    try
      let path = "tmp.open"
      let filepath = FilePath(h.env.root as AmbientAuth, path)?
      with file = CreateFile(filepath) as File do
        file.print("foobar")
      end
      with file2 = OpenFile(filepath) as File do
        let line1 = file2.line()?
        h.assert_eq[String]("foobar", consume line1)
        h.assert_true(file2.valid())
        h.assert_false(file2.writeable)
      else
        h.fail("Failed read on opened file!")
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end


class iso _TestFileOpenError is UnitTest
  fun name(): String => "files/File.open-error"
  fun apply(h: TestHelper) =>
    try
      let path = "tmp.openerror"
      let filepath = FilePath(h.env.root as AmbientAuth, path)?
      h.assert_false(filepath.exists())
      let file = OpenFile(filepath)
      h.assert_true(file is FileError)
    else
      h.fail("Unhandled error!")
    end


class _TestFileOpenWrite is UnitTest
  fun name(): String => "files/File.open.write"
  fun apply(h: TestHelper) =>
    try
      let path = "tmp.open-write"
      let filepath = FilePath(h.env.root as AmbientAuth, path)?
      with file = CreateFile(filepath) as File do
        file.print("write on file opened read-only")
      end
      h.assert_true(filepath.exists())
      with opened = File.open(filepath) do
        h.assert_is[FileErrNo](FileOK, opened.errno())
        h.assert_true(opened.valid(), "read-only file not marked as valid")
        h.assert_false(opened.writeable)
        h.assert_false(opened.write("oh, noes!"))
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end


class iso _TestFileOpenPermissionDenied is UnitTest
  fun name(): String => "files/File.open-permission-denied"
  fun apply(h: TestHelper) =>
    try
      let filepath = FilePath(h.env.root as AmbientAuth, "tmp.open-not-readable")?
      with file = CreateFile(filepath) as File do
        file.print("unreadable")
      end
      let mode: FileMode ref = FileMode.>private()
      mode.owner_read = false
      mode.owner_write = false
      h.assert_true(filepath.chmod(mode))
      let opened = File.open(filepath)
      h.assert_true(opened.errno() is FilePermissionDenied)
      h.assert_false(opened.valid())
      let read = opened.read(10)
      h.assert_eq[USize](read.size(), 0)

      mode.owner_read = true
      mode.owner_write = true
      filepath.chmod(mode)
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end


class iso _TestFileLongLine is UnitTest
  fun name(): String => "files/File.longline"
  fun apply(h: TestHelper) =>
    try
      let path = "tmp.longline"
      let filepath = FilePath(h.env.root as AmbientAuth, path)?
      with file = File(filepath) do
        var longline = "foobar"
        for d in Range(0, 10) do
          longline = longline + longline
        end
        file.print(longline)
        file.sync()
        file.seek_start(0)
        let line1 = file.line()?
        h.assert_eq[String](longline, consume line1)
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end

class iso _TestFileWrite is UnitTest
  fun name(): String => "files/File.write"
  fun apply(h: TestHelper) =>
    try
      let path = "tmp.write"
      let filepath = FilePath(h.env.root as AmbientAuth, path)?
      with file = CreateFile(filepath) as File do
        file.write("foobar\n")
      end
      with file2 = CreateFile(filepath) as File do
        let line1 = file2.line()?
        h.assert_eq[String]("foobar", consume line1)
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end

class iso _TestFileWritev is UnitTest
  fun name(): String => "files/File.writev"
  fun apply(h: TestHelper) =>
    try
      let wb: Writer ref = Writer
      let line1 = "foobar\n"
      let line2 = "barfoo\n"
      wb.write(line1)
      wb.write(line2)
      let path = "tmp.writev"
      let filepath = FilePath(h.env.root as AmbientAuth, path)?
      with file = CreateFile(filepath) as File do
        file.writev(wb.done())
      end
      with file2 = CreateFile(filepath) as File do
        let fileline1 = file2.line()?
        let fileline2 = file2.line()?
        h.assert_eq[String](line1.split("\n")(0)?, consume fileline1)
        h.assert_eq[String](line2.split("\n")(0)?, consume fileline2)
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end

class iso _TestFileQueue is UnitTest
  fun name(): String => "files/File.queue"
  fun apply(h: TestHelper) =>
    try
      let path = "tmp.queue"
      let filepath = FilePath(h.env.root as AmbientAuth, path)?
      with file = CreateFile(filepath) as File do
        file.queue("foobar\n")
      end
      with file2 = CreateFile(filepath) as File do
        let line1 = file2.line()?
        h.assert_eq[String]("foobar", consume line1)
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end

class iso _TestFileQueuev is UnitTest
  fun name(): String => "files/File.queuev"
  fun apply(h: TestHelper) =>
    try
      let wb: Writer ref = Writer
      let line1 = "foobar\n"
      let line2 = "barfoo\n"
      wb.write(line1)
      wb.write(line2)
      let path = "tmp.queuev"
      let filepath = FilePath(h.env.root as AmbientAuth, path)?
      with file = CreateFile(filepath) as File do
        file.queuev(wb.done())
      end
      with file2 = CreateFile(filepath) as File do
        let fileline1 = file2.line()?
        let fileline2 = file2.line()?
        h.assert_eq[String](line1.split("\n")(0)?, consume fileline1)
        h.assert_eq[String](line2.split("\n")(0)?, consume fileline2)
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end

class iso _TestFileMixedWriteQueue is UnitTest
  fun name(): String => "files/File.mixedwrite"
  fun apply(h: TestHelper) =>
    try
      let wb: Writer ref = Writer
      let line1 = "foobar\n"
      let line2 = "barfoo\n"
      let line3 = "foobar2"
      let line4 = "barfoo2"
      let line5 = "foobar3\n"
      let line6 = "barfoo3\n"
      wb.write(line1)
      wb.write(line2)
      let writev_data = wb.done()
      wb.write(line3)
      wb.write(line4)
      let printv_data = wb.done()
      wb.write(line5)
      wb.write(line6)
      let queuev_data = wb.done()
      let path = "tmp.mixedwrite"
      let filepath = FilePath(h.env.root as AmbientAuth, path)?
      with file = CreateFile(filepath) as File do
        file.print(line3)
        file.queue(line5)
        file.write(line1)
        file.printv(consume printv_data)
        file.queuev(consume queuev_data)
        file.writev(consume writev_data)
      end
      with file2 = CreateFile(filepath) as File do
        h.assert_eq[String](line3, file2.line()?)
        h.assert_eq[String](line5.split("\n")(0)?, file2.line()?)
        h.assert_eq[String](line1.split("\n")(0)?, file2.line()?)
        h.assert_eq[String](line3, file2.line()?)
        h.assert_eq[String](line4, file2.line()?)
        h.assert_eq[String](line5.split("\n")(0)?, file2.line()?)
        h.assert_eq[String](line6.split("\n")(0)?, file2.line()?)
        h.assert_eq[String](line1.split("\n")(0)?, file2.line()?)
        h.assert_eq[String](line2.split("\n")(0)?, file2.line()?)
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end

class iso _TestFileWritevLarge is UnitTest
  fun name(): String => "files/File.writevlarge"
  fun apply(h: TestHelper) =>
    try
      let wb: Writer ref = Writer
      let writev_batch_size: USize = 10 + @pony_os_writev_max[I32]().usize()
      var count: USize = 0
      while count < writev_batch_size do
        wb.write(count.string() + "\n")
        count = count + 1
      end
      let path = "tmp.writevlarge"
      let filepath = FilePath(h.env.root as AmbientAuth, path)?
      with file = CreateFile(filepath) as File do
        file.writev(wb.done())
      end
      with file2 = CreateFile(filepath) as File do
        count = 0
        while count < writev_batch_size do
          let fileline1 = file2.line()?
          h.assert_eq[String](count.string(), consume fileline1)
          count = count + 1
        end
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end
