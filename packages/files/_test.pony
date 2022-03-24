use "pony_test"
use "collections"
use "buffered"
use "term"
use "random"
use "time"

use @getuid[U32]() if not windows

actor \nodoc\ Main is TestList
  new create(env: Env) => PonyTest(env, this)

  new make() => None

  fun tag tests(test: PonyTest) =>
    // Tests below function across all systems and are listed alphabetically
    test(_TestDirectoryFileOpen)
    test(_TestDirectoryOpen)
    test(_TestDirectoryRemoveReadOnly)
    test(_TestFileCreate)
    test(_TestFileCreateExistsNotWriteable)
    test(_TestFileCreateMissingCaps)
    test(_TestFileEOF)
    test(_TestFileFlush)
    test(_TestFileLinesEmptyFile)
    test(_TestFileLinesMovingCursor)
    test(_TestFileLinesMultiLine)
    test(_TestFileLinesSingleLine)
    test(_TestFileLongLine)
    test(_TestFileMixedWriteQueue)
    test(_TestFileOpen)
    test(_TestFileOpenError)
    test(_TestFileOpenWrite)
    test(_TestFilePathFileAuth)
    test(_TestFilePathFrom)
    test(_TestFilePathFromError)
    test(_TestFileQueue)
    test(_TestFileQueuev)
    test(_TestFileReadMore)
    test(_TestFileRemoveReadOnly)
    test(_TestFileWrite)
    test(_TestFileWritev)
    test(_TestFileWritevLarge)
    test(_TestMkdtemp)
    test(_TestPathBase)
    test(_TestPathClean)
    test(_TestPathDir)
    test(_TestPathExt)
    test(_TestPathJoin)
    test(_TestPathRel)
    test(_TestPathSplit)
    test(_TestPathVolume)
    test(_TestWalk)

    // Tests below exclude windows and are listed alphabetically
    ifdef not windows then
      test(_TestFileCreateDirNotWriteable)
      test(_TestFileOpenInDirNotWriteable)
      test(_TestFileOpenPermissionDenied)
      test(_TestPathRoot)
      test(_TestSymlink)
    end

primitive \nodoc\ _FileHelper
  fun make_files(h: TestHelper, files: Array[String]): FilePath ? =>
    let top = Directory(FilePath.mkdtemp(FileAuth(h.env.root),
      "tmp._FileHelper.")?)?
    for f in files.values() do
      try
        // Since we embed paths, we use the posix separator, even on Windows.
        let dir_head = Path.split(f, "/")
        let fp = FilePath.from(top.path, dir_head._1)?
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

primitive \nodoc\ _GetUID
  fun apply(): U32 =>
    ifdef not windows then
      @getuid()
    else
      compile_error "getuid not available on windows"
    end

trait \nodoc\ iso _NonRootTest is UnitTest

  fun apply_as_non_root(h: TestHelper) ?

  fun apply(h: TestHelper) ? =>
    if runs_as_root(h) then
      h.env.err.print(
        ANSI.red() + ANSI.bold() +
        "[" + name() + "] " +
        "This test is disabled as it cannot be run as root." +
        ANSI.reset())
    else
      apply_as_non_root(h)?
    end

  fun runs_as_root(h: TestHelper): Bool =>
    if h.env.vars.contains("USER=root") then
      true
    else
      ifdef not windows then
        _GetUID() == 0
      else
        false
      end
    end

class \nodoc\ iso _TestMkdtemp is UnitTest
  fun name(): String => "files/FilePath.mkdtemp"
  fun apply(h: TestHelper) ? =>
    let tmp = FilePath.mkdtemp(FileAuth(h.env.root), "tmp.TestMkdtemp.")?
    try
      h.assert_true(FileInfo(tmp)?.directory)
    then
      h.assert_true(tmp.remove())
    end

class \nodoc\ iso _TestWalk is UnitTest
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

class \nodoc\ iso _TestSymlink is UnitTest
  var tmp_dir: (FilePath | None) = None

  fun ref set_up(h: TestHelper) ? =>
    tmp_dir = FilePath.mkdtemp(FileAuth(h.env.root), "symlink")?

  fun ref tear_down(h: TestHelper) =>
    try
      (tmp_dir as FilePath).remove()
    end

  fun name(): String => "files/FilePath.symlink"

  fun ref apply(h: TestHelper) ? =>
    test_file(h)?
    test_directory(h)?

  fun ref test_file(h: TestHelper) ? =>
    let target_path = (tmp_dir as FilePath).join("target_file")?
    let content = "abcdef"
    with target_file = CreateFile(target_path) as File do
      h.assert_true(
        target_file.write(content),
        "could not write to file: " + target_path.path)
    end

    let link_path = (tmp_dir as FilePath).join("symlink_file")?
    h.assert_true(
      target_path.symlink(link_path),
      "could not create symbolic link to: " + target_path.path)

    with link_file = OpenFile(link_path) as File do
      let fl = FileLines(link_file)
      h.assert_true(fl.has_next())
      h.assert_eq[String](fl.next()?, content)
    end

  fun ref test_directory(h: TestHelper) ? =>
    let target_path = (tmp_dir as FilePath).join("target_dir")?
    h.assert_true(target_path.mkdir(true))

    let link_path = (tmp_dir as FilePath).join("symlink_dir")?
    h.assert_true(
      target_path.symlink(link_path),
      "could not create symbolic link to: " + target_path.path)

class \nodoc\ iso _TestFilePathFileAuth is UnitTest
  """
  _TestFilePathFileAuth checks that it is error-free to produce a filepath
  using `FileAuth` created from `AmbientAuth`.
  """
  fun name(): String => "files/FilePath.create-w-fileauth"
  fun apply(h: TestHelper) =>
    let path = "tmp.filepath"
    let filepath = FilePath(FileAuth(h.env.root), path)
    h.assert_no_error({()? => FilePath.from(filepath, path)? })

class \nodoc\ iso _TestFilePathFrom is UnitTest
  """
  _TestFilePathFrom checks that it is error-free to produce a filepath
  using the from constructor to the same path of the provided `FilePath`.

  That is, a FilePath to some user file/directory can then be used to produce
  a FilePath to the same file/directory.
  """
  fun name(): String => "files/FilePath.from-success"
  fun apply(h: TestHelper) =>
    let path = "tmp.filepath"
    let filepath = FilePath(FileAuth(h.env.root), path)
    h.assert_no_error({()? => FilePath.from(filepath, path)? })

class \nodoc\ iso _TestFilePathFromError is UnitTest
  """
  _TestFilePathFromError checks that it is an error to produce a filepath
  using the from constructor which is not a subpath of the provided `FilePath`.

  That is, a FilePath to some user file/directory cannot be used to create a FilePath to
  the root directory.
  """
  fun name(): String => "files/FilePath.from-error"
  fun apply(h: TestHelper) =>
    let path = "tmp.filepath"
    let filepath = FilePath(FileAuth(h.env.root), path)
    h.assert_error({()? => FilePath.from(filepath, "/")? })

class \nodoc\ iso _TestDirectoryOpen is UnitTest
  fun name(): String => "files/File.open.directory"
  fun apply(h: TestHelper) ? =>
    let tmp = FilePath.mkdtemp(FileAuth(h.env.root), "tmp.TestDiropen.")?

    try
      h.assert_true(FileInfo(tmp)?.directory)
      with file = File.open(tmp) do
        h.assert_true(file.errno() is FileError)
        h.assert_false(file.valid())
      end
    then
      h.assert_true(tmp.remove())
    end

class \nodoc\ iso _TestDirectoryFileOpen is UnitTest
  fun name(): String => "files/Directory.open-file"
  fun apply(h: TestHelper) =>
  try
    // make a temporary directory
    let dir_path = FilePath.mkdtemp(
      FileAuth(h.env.root),
      "tmp.directory.open-file")?
    try
      let dir = Directory(dir_path)?

      // create a file (rw)
      let created: File = dir.create_file("created")?
      h.assert_true(created.valid())
      created.dispose()

      // open a file (ro)
      let readonly: File = dir.open_file("created")?
      h.assert_true(readonly.valid())
      readonly.dispose()
    else
      h.fail("Unhandled inner error!")
    then
      dir_path.remove()
    end
  else
    h.fail("Unhandled error!")
  end

class \nodoc\ iso _TestPathClean is UnitTest
  fun name(): String => "files/Path.clean"
  fun apply(h: TestHelper) =>
    let res1 = Path.clean("//foo/bar///")
    let res2 = Path.clean("foo/./bar")
    let res3 = Path.clean("foo/../foo")
    let res4 = Path.clean("///foo///bar///base.ext")
    let res5 = Path.clean("/foo/bar/baz/../..")
    let res6 = Path.clean("/foo/bar/baz/..")
    let res7 = Path.clean("/foo/bar/baz/../../qux")

    ifdef windows then
      h.assert_eq[String](res1, "\\foo\\bar")
      h.assert_eq[String](res2, "foo\\bar")
      h.assert_eq[String](res3, "foo")
      h.assert_eq[String](res4, "\\foo\\bar\\base.ext")
      h.assert_eq[String](res5, "\\foo")
      h.assert_eq[String](res6, "\\foo\\bar")
      h.assert_eq[String](res7, "\\foo\\qux")
    else
      h.assert_eq[String](res1, "/foo/bar")
      h.assert_eq[String](res2, "foo/bar")
      h.assert_eq[String](res3, "foo")
      h.assert_eq[String](res4, "/foo/bar/base.ext")
      h.assert_eq[String](res5, "/foo")
      h.assert_eq[String](res6, "/foo/bar")
      h.assert_eq[String](res7, "/foo/qux")
    end

class \nodoc\ iso _TestPathJoin is UnitTest
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

class \nodoc\ iso _TestPathRel is UnitTest
  fun name(): String => "files/Path.rel"
  fun apply(h: TestHelper) ? =>
    let res = Path.rel("foo/bar", "foo/bar/baz")?
    h.assert_eq[String](res, "baz")

class \nodoc\ iso _TestPathSplit is UnitTest
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

class \nodoc\ iso _TestPathDir is UnitTest
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

class \nodoc\ iso _TestPathBase is UnitTest
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

class \nodoc\ iso _TestPathExt is UnitTest
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

class \nodoc\ iso _TestPathVolume is UnitTest
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

class \nodoc\ iso _TestPathRoot is UnitTest
  fun name(): String => "files/Path.root"
  fun apply(h: TestHelper) =>
    let res1 = Path.abs("/")
    let res2 = Path.abs("/foo/../")
    h.assert_eq[String](res1, "/")
    h.assert_eq[String](res2, "/")

class \nodoc\ iso _TestFileEOF is UnitTest
  fun name(): String => "files/File.eof-error"
  fun apply(h: TestHelper) =>
    let path = "tmp.eof"
    let filepath = FilePath(FileAuth(h.env.root), path)
    with file = File(filepath) do
      file.write("foobar")
      file.sync()
      file.seek_start(0)
      let line1 = file.read_string(6)
      h.assert_eq[String]("foobar", consume line1)

      let line2 = file.read_string(1)
      h.assert_eq[USize](line2.size(), 0, "Read beyond EOF without error!")
      h.assert_true(file.errno() is FileEOF)
    end
    filepath.remove()

class \nodoc\ iso _TestFileCreate is UnitTest
  fun name(): String => "files/File.create"
  fun apply(h: TestHelper) =>
    try
      let path = "tmp.create"
      let filepath = FilePath(FileAuth(h.env.root), path)
      with file = CreateFile(filepath) as File do
        file.print("foobar")
      end
      with file2 = CreateFile(filepath) as File do
        h.assert_eq[String]("foobar", "".join(file2.lines()))
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end

class \nodoc\ iso _TestFileCreateExistsNotWriteable is _NonRootTest
  fun name(): String => "files/File.create-exists-not-writeable"
  fun apply_as_non_root(h: TestHelper) ? =>
    let content = "unwriteable"
    let path = "tmp.create-not-writeable"
    let filepath = FilePath(FileAuth(h.env.root), path)
    let mode: FileMode ref = FileMode.>private()
    mode.owner_read = true
    mode.owner_write = false

    // preparing the non-writable, but readable file
    with file = CreateFile(filepath) as File do
      file.write(content)
    end

    h.assert_true(filepath.chmod(mode))

    with file2 = File(filepath) do
      h.assert_false(file2.valid())
      h.assert_is[FileErrNo](file2.errno(), FilePermissionDenied)

      let line = file2.read(6)
      h.assert_eq[USize](0, line.size(), "read on invalid file succeeded")
    end

    h.assert_true(filepath.remove())

class \nodoc\ iso _TestFileCreateDirNotWriteable is _NonRootTest
  fun name(): String => "files/File.create-dir-not-writeable"
  fun apply_as_non_root(h: TestHelper) =>
    ifdef not windows then
      try
        let dir_path =
          FilePath.mkdtemp(
            FileAuth(h.env.root),
            "tmp.create-dir-not-writeable")?
        let mode: FileMode ref = FileMode.>private()
        mode.owner_read = true
        mode.owner_write = false
        mode.owner_exec = false
        h.assert_true(dir_path.chmod(mode))

        try
          let file_path = dir_path.join("trycreateme")?
          let file = File(file_path)

          h.assert_false(file.valid())
          h.assert_true(file.writeable)
          h.assert_is[FileErrNo](file.errno(), FilePermissionDenied)
        then
          mode.owner_write = true
          mode.owner_exec = true
          h.assert_true(dir_path.chmod(mode))
          dir_path.remove()
        end
      else
        h.fail("Unhandled error!")
      end
    end

class \nodoc\ iso _TestFileOpenInDirNotWriteable is UnitTest
  fun name(): String => "files/File.open-dir-not-writeable"
  fun apply(h: TestHelper) =>
    ifdef not windows then
      try
        // make a temporary directory
        let dir_path = FilePath.mkdtemp(
          FileAuth(h.env.root),
          "tmp.open-dir-not-writeable")?
        try
          let dir = Directory(dir_path)?

          // create a file (rw)
          let created: File = dir.create_file("created")?
          h.assert_true(created.valid())
          h.assert_true(created.writeable)
          created.dispose()

          // open a file (ro)
          let readonly: File = dir.open_file("created")?
          h.assert_true(readonly.valid())
          h.assert_false(readonly.writeable)
          readonly.dispose()
        else
          h.fail("Unhandled inner error!")
        then
          dir_path.remove()
        end
      else
        h.fail("Unhandled error!")
      end
    end

class \nodoc\ iso _TestFileCreateMissingCaps is UnitTest
  fun name(): String => "files/File.create-missing-caps"
  fun apply(h: TestHelper) =>
    let no_create_caps = FileCaps.>all().>unset(FileCreate)
    let no_read_caps = FileCaps.>all().>unset(FileWrite)
    let no_write_caps = FileCaps.>all().>unset(FileRead)

    let file_path1 = FilePath(
      FileAuth(h.env.root),
      "tmp.create-missing-caps1",
      consume no_create_caps)
    let file1 = File(file_path1)
    h.assert_false(file1.valid())
    h.assert_is[FileErrNo](file1.errno(), FileError)

    let file_path2 = FilePath(
      FileAuth(h.env.root),
      "tmp.create-missing-caps2",
      consume no_read_caps)
    let file2 = File(file_path2)
    h.assert_false(file2.valid())
    h.assert_is[FileErrNo](file2.errno(), FileError)

    let file_path3 = FilePath(
      FileAuth(h.env.root),
      "tmp.create-missing-caps3",
      consume no_write_caps)
    let file3 = File(file_path3)
    h.assert_false(file3.valid())
    h.assert_is[FileErrNo](file3.errno(), FileError)

class \nodoc\ iso _TestFileOpen is UnitTest
  fun name(): String => "files/File.open"
  fun apply(h: TestHelper) =>
    try
      let path = "tmp.open"
      let filepath = FilePath(FileAuth(h.env.root), path)
      with file = CreateFile(filepath) as File do
        file.print("foobar")
      end
      try
        let file2 = OpenFile(filepath) as File
        h.assert_eq[String]("foobar", "".join(file2.lines()))
        h.assert_true(file2.valid())
        h.assert_false(file2.writeable)
        file2.dispose()
      else
        h.fail("Failed read on opened file!")
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end

class \nodoc\ iso _TestFileOpenError is UnitTest
  fun name(): String => "files/File.open-error"
  fun apply(h: TestHelper) =>
    let path = "tmp.openerror"
    let filepath = FilePath(FileAuth(h.env.root), path)
    h.assert_false(filepath.exists())
    let file = OpenFile(filepath)
    h.assert_true(file is FileError)

class _TestFileOpenWrite is UnitTest
  fun name(): String => "files/File.open.write"
  fun apply(h: TestHelper) =>
    try
      let path = "tmp.open-write"
      let filepath = FilePath(FileAuth(h.env.root), path)
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

class \nodoc\ iso _TestFileOpenPermissionDenied is _NonRootTest
  fun name(): String => "files/File.open-permission-denied"
  fun apply_as_non_root(h: TestHelper) =>
    ifdef not windows then
        // on windows all files are always writeable
        // with chmod there is no way to make a file not readable
      try
        let filepath = FilePath(FileAuth(h.env.root), "tmp.open-not-readable")
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
    end

class \nodoc\ iso _TestFileLongLine is UnitTest
  fun name(): String => "files/File.longline"
  fun apply(h: TestHelper) =>
    let path = "tmp.longline"
    let filepath = FilePath(FileAuth(h.env.root), path)
    with file = File(filepath) do
      var longline = "foobar"
      for d in Range(0, 10) do
        longline = longline + longline
      end
      file.print(longline)
      file.sync()
      file.seek_start(0)
      let line1 = file.read_string(longline.size())
      h.assert_eq[String](longline, consume line1)
    end
    filepath.remove()

class \nodoc\ iso _TestFileWrite is UnitTest
  fun name(): String => "files/File.write"
  fun apply(h: TestHelper) =>
    try
      let path = "tmp.write"
      let filepath = FilePath(FileAuth(h.env.root), path)
      with file = CreateFile(filepath) as File do
        file.write("foobar\n")
      end
      with file2 = CreateFile(filepath) as File do
        let line1 = file2.read_string(8)
        h.assert_eq[String]("foobar\n", consume line1)
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end

class \nodoc\ iso _TestFileWritev is UnitTest
  fun name(): String => "files/File.writev"
  fun apply(h: TestHelper) =>
    try
      let wb: Writer ref = Writer
      let line1 = "foobar\n"
      let line2 = "barfoo\n"
      wb.write(line1)
      wb.write(line2)
      let path = "tmp.writev"
      let filepath = FilePath(FileAuth(h.env.root), path)
      with file = CreateFile(filepath) as File do
        h.assert_true(file.writev(wb.done()))
      end
      with file2 = CreateFile(filepath) as File do
        h.assert_eq[String](
          "foobar barfoo",
          " ".join(file2.lines())
        )
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end

class \nodoc\ iso _TestFileQueue is UnitTest
  fun name(): String => "files/File.queue"
  fun apply(h: TestHelper) =>
    try
      let path = "tmp.queue"
      let filepath = FilePath(FileAuth(h.env.root), path)
      with file = CreateFile(filepath) as File do
        file.queue("foobar\n")
      end
      with file2 = CreateFile(filepath) as File do
        h.assert_eq[String]("foobar", "".join(file2.lines()))
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end

class \nodoc\ iso _TestFileQueuev is UnitTest
  fun name(): String => "files/File.queuev"
  fun apply(h: TestHelper) =>
    try
      let wb: Writer ref = Writer
      let line1 = "foobar\n"
      let line2 = "barfoo\n"
      wb.write(line1)
      wb.write(line2)
      let path = "tmp.queuev"
      let filepath = FilePath(FileAuth(h.env.root), path)
      with file = CreateFile(filepath) as File do
        file.queuev(wb.done())
      end
      with file2 = CreateFile(filepath) as File do
        h.assert_eq[String]("foobar barfoo", " ".join(file2.lines()))
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end

class \nodoc\ iso _TestFileMixedWriteQueue is UnitTest
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
      let filepath = FilePath(FileAuth(h.env.root), path)
      with file = CreateFile(filepath) as File do
        file.print(line3)
        file.queue(line5)
        file.write(line1)
        file.printv(consume printv_data)
        file.queuev(consume queuev_data)
        file.writev(consume writev_data)
      end
      with file2 = CreateFile(filepath) as File do
        h.assert_eq[String](
          "".join([
            line3 + "\n"
            line5
            line1
            line3
            line4 + "\n"
            line5
            line6
            line1
            line2
          ].values()),
          file2.read_string(256))
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end

class \nodoc\ iso _TestFileWritevLarge is UnitTest
  fun name(): String => "files/File.writevlarge"
  fun apply(h: TestHelper) =>
    try
      let wb: Writer ref = Writer
      let writev_batch_size: USize = 10 + @pony_os_writev_max().usize()
      var count: USize = 0
      while count < writev_batch_size do
        wb.write(count.string() + "\n")
        count = count + 1
      end
      let path = "tmp.writevlarge"
      let filepath = FilePath(FileAuth(h.env.root), path)
      with file = CreateFile(filepath) as File do
        h.assert_true(file.writev(wb.done()))
      end
      with file2 = CreateFile(filepath) as File do
        count = 0
        for line in file2.lines() do
          h.assert_eq[String](count.string(), consume line)
          count = count + 1
          h.log(count.string())
        end
      end
      filepath.remove()
    else
      h.fail("Unhandled error!")
    end

class \nodoc\ iso _TestFileFlush is UnitTest
  fun name(): String => "files/File.flush"
  fun apply(h: TestHelper) =>
    try
      let path = FilePath(FileAuth(h.env.root), "tmp.flush")
      with file = CreateFile(path) as File do
        // Flush with no writes succeeds trivially, but does nothing.
        h.assert_true(file.flush())

        file.queue("foobar\n")

        // Without flushing, the file size is still zero.
        with read_file = CreateFile(path) as File do
          h.assert_eq[USize](0, read_file.size())
        end

        h.assert_true(file.flush())

        // Now expect to be able to see the data.
        with read_file = CreateFile(path) as File do
          h.assert_eq[String]("foobar", "".join(read_file.lines()))
        end
      end
      path.remove()
    else
      h.fail("Unhandled error!")
    end

class \nodoc\ iso _TestFileReadMore is UnitTest
  fun name(): String => "files/File.read-more"
  fun apply(h: TestHelper)? =>
    let path = FilePath(FileAuth(h.env.root), "tmp-read-more")
    with file = CreateFile(path) as File do
      h.assert_true(file.write("foobar"))
    end

    with read_file = OpenFile(path) as File do
      let content = read_file.read(10)
      h.assert_eq[USize](6, content.size())
      h.assert_is[FileErrNo](
        read_file.errno(),
        FileOK,
        "File errno is not OK after reading fewer bytes than requested")
      h.assert_eq[USize](0, read_file.read(10).size())
      h.assert_is[FileErrNo](
        read_file.errno(),
        FileEOF,
        "File errno is not EOF after reading past the last byte")
    end
    path.remove()

class \nodoc\ iso _TestFileRemoveReadOnly is UnitTest
  fun name(): String => "files/File.remove-readonly-file"
  fun apply(h: TestHelper) ? =>
    let path = FilePath(FileAuth(h.env.root), "tmp-read-only")
    with file = CreateFile(path) as File do
      None
    end

    let mode: FileMode ref = FileMode
    mode.owner_read = true
    mode.owner_write = false
    mode.group_read = true
    mode.group_write = false
    mode.any_read = true
    mode.any_write = false
    h.assert_true(path.chmod(mode))
    h.assert_true(path.remove())

class \nodoc\ iso _TestDirectoryRemoveReadOnly is UnitTest
  fun name(): String => "files/File.remove-readonly-directory"

  fun apply(h: TestHelper) ? =>
    let path = FilePath.mkdtemp(FileAuth(h.env.root), "tmp-read-only-dir")?
    let dir = Directory(path)?
    let mode: FileMode ref = FileMode
    mode.owner_read = true
    mode.owner_write = false
    mode.owner_exec = true
    mode.group_read = true
    mode.group_write = false
    mode.group_exec = true
    mode.any_read = true
    mode.any_write = false
    mode.any_exec = true
    h.assert_true(path.chmod(mode))
    h.assert_true(path.remove())

class \nodoc\ iso _TestFileLinesEmptyFile is UnitTest
  var tmp_dir: (FilePath | None) = None

  fun ref set_up(h: TestHelper) ? =>
    tmp_dir = FilePath.mkdtemp(FileAuth(h.env.root), "empty")?

  fun ref tear_down(h: TestHelper) =>
    try (tmp_dir as FilePath).remove() end

  fun name(): String => "files/FileLines.empty"

  fun ref apply(h: TestHelper) ? =>
    let tmp_file = (tmp_dir as FilePath).join("empty")?
    with file = CreateFile(tmp_file) as File do
      file.write(Array[U8])
    end

    with f = OpenFile(tmp_file) as File do
      let fl = FileLines(f)
      var lines_returned: USize = 0
      for _ in fl do
        lines_returned = lines_returned + 1
      end
      h.assert_eq[USize](lines_returned, 0, "FileLines returned a line for an empty file")
    end

class \nodoc\ iso _TestFileLinesSingleLine is UnitTest

  let lines: Array[String] = [ as String:
    "a"
    "a\n"
    "a\r\n"
    "abcd"
    "ABCD\n"
    "ABCD\r\n"
    String.from_array(recover val Array[U8].init('a', 255) end)
    String.from_array(recover val Array[U8].init('a', 255) end) + "\n"
    String.from_array(recover val Array[U8].init('a', 255) end) + "\r\n"
    String.from_array(recover val Array[U8].init('b', 256) end)
    String.from_array(recover val Array[U8].init('b', 256) end) + "\n"
    String.from_array(recover val Array[U8].init('b', 256) end) + "\r\n"
    String.from_array(recover val Array[U8].init('c', 257) end)
    String.from_array(recover val Array[U8].init('c', 257) end) + "\n"
    String.from_array(recover val Array[U8].init('c', 257) end) + "\r\n"
    String.from_array(recover val Array[U8].init('d', 100_000) end)
  ]

  var tmp_dir: (FilePath | None) = None

  fun ref set_up(h: TestHelper) ? =>
    tmp_dir = FilePath.mkdtemp(FileAuth(h.env.root), "single-line")?

  fun ref tear_down(h: TestHelper) =>
    try
      (tmp_dir as FilePath).remove()
    end

  fun name(): String => "files/FileLines.single_line"

  fun ref apply(h: TestHelper)? =>
    var i: USize = 0
    for line in lines.values() do
      let tmp_file = (tmp_dir as FilePath).join("single-line-" + i.string())?
      with file = CreateFile(tmp_file) as File do
        h.assert_true(
          file.write(line),
          "could not write to file: " + tmp_file.path)
      end

      with file = OpenFile(tmp_file) as File do
        let fl = FileLines(file)
        var lines_returned: USize = 0
        for read_line in fl do
          let compare_line =
            if line.contains("\r\n") then
              line.substring(0, line.size().isize() - 2)
            elseif line.contains("\n") then
              line.substring(0, line.size().isize() -1)
            else
              line
            end
          h.assert_eq[String](consume read_line, compare_line)
          lines_returned = lines_returned + 1
        end
        h.assert_eq[USize](lines_returned, 1,
          "FileLines returned " + lines_returned.string() +
          " for single line: '" + line + "'")
        h.assert_eq[USize](file.position(), line.size(),
          "FileLines advanced the file cursor to " + file.position().string() +
          " though the line has " + line.size().string() + " bytes.")
      end
      i = i + 1
    end

class \nodoc\ _TestFileLinesMultiLine is UnitTest
  var tmp_dir: (FilePath | None) = None

  let line_endings: Array[String] val = ["\n"; "\r\n"]
  let file_contents: Array[(Array[String] val, USize)] val = [
    (["a"; "b"], 2)
    (["a"; ""; "b"], 3)
    (["a"; "b"; ""], 2)
    ([""; "b"; "c"], 3)
    ([""; ""], 1)
    ([""; " "], 2)
    ([""; ""; ""], 2)
    ([
      String.from_array(recover val Array[U8].init('a', 254) end)
      String.from_array(recover val Array[U8].init('a', 257) end)], 2)
    ([
      String.from_array(recover val Array[U8].init('b', 256) end)
      ""
      String.from_array(recover val Array[U8].init('c', 256) end)
      ], 3)
  ]

  fun ref set_up(h: TestHelper) ? =>
    tmp_dir = FilePath.mkdtemp(FileAuth(h.env.root), "multi-line")?

  fun ref tear_down(h: TestHelper) =>
    try
      (tmp_dir as FilePath).remove()
    end

  fun name(): String => "files/FileLines.multi_line"

  fun ref apply(h: TestHelper)? =>
    var i: USize = 0
    for lines_and_count in file_contents.values() do
      let lines = lines_and_count._1
      for line_ending in line_endings.values() do
        let tmp_file = (tmp_dir as FilePath).join("multi-line-" + i.string())?
        let content = recover val line_ending.join(lines.values()) end

        with file = CreateFile(tmp_file) as File do
          h.assert_true(
            file.write(content),
            "could not write to file: " + tmp_file.path)
        end

        with file = OpenFile(tmp_file) as File do
          let fl = FileLines(file)
          var lines_returned: USize = 0
          for read_line in fl do
            lines_returned = lines_returned + 1
          end
          let expected_line_count = lines_and_count._2
          h.assert_eq[USize](
            lines_returned,
            expected_line_count,
            "FileLines returned " + lines_returned.string() + " (expected " +
            expected_line_count.string() + ") lines for file
            content:\n----\n" + content + "\n----\n")
        end
        i = i + 1
      end
    end

class \nodoc\ _TestFileLinesMovingCursor is UnitTest
  var tmp_dir: (FilePath | None) = None

  fun ref set_up(h: TestHelper) ? =>
    tmp_dir = FilePath.mkdtemp(FileAuth(h.env.root), "moving-cursor")?

  fun ref tear_down(h: TestHelper) =>
    try
      (tmp_dir as FilePath).remove()
    end

  fun name(): String => "files/FileLines.moving_cursor"

  fun ref apply(h: TestHelper)? =>
    let tmp_file = (tmp_dir as FilePath).join("moving-cursor")?
    let content = "a\nb\nc\nd"
    with file = CreateFile(tmp_file) as File do
      h.assert_true(
        file.write(content),
        "could not write to file: " + tmp_file.path)
    end

    with file = OpenFile(tmp_file) as File do
      h.assert_eq[USize](file.position(), 0)
      let fl1 = FileLines(file)
      h.assert_eq[String](" ".join(fl1), "a b c d")
      h.assert_eq[USize](file.position(), content.size())

      file.seek_start(2)
      let fl2 = FileLines(file)
      h.assert_eq[String](" ".join(fl2), "b c d")
      h.assert_eq[USize](file.position(), content.size())

      file.seek_start(0)
      let fl3 = FileLines(file)
      file.seek_start(3)
      h.assert_true(fl3.has_next())
      h.assert_eq[String](fl3.next()?, "a")
      h.assert_eq[USize](file.position(), 2)
      file.seek_start(5)
      h.assert_true(fl3.has_next())
      h.assert_eq[String](fl3.next()?, "b")
      h.assert_eq[USize](file.position(), 4)
      file.seek_start(0)
      h.assert_true(fl3.has_next())
      h.assert_eq[String](fl3.next()?, "c")
      h.assert_eq[USize](file.position(), 6)
      file.seek_start(10)
      h.assert_true(fl3.has_next())
      h.assert_eq[String](fl3.next()?, "d")
      h.assert_eq[USize](file.position(), 7)
    end
